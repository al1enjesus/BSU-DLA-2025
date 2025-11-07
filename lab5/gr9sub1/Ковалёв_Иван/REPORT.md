# Лабораторная работа 5
## Модули ядра Linux

---

## Содержание
1. [Введение](#введение)
2. [Задание A: Hello World модуль](#задание-a-hello-world-модуль)
3. [Задание B: /proc файл с информацией](#задание-b-proc-файл-с-информацией)
4. [Задание C: Простой character device](#задание-c-простой-character-device)
5. [Ответы на вопросы](#ответы-на-вопросы)
6. [Заключение](#заключение)

---

## Введение

**Цель работы:** Изучить архитектуру ядра Linux, написать простые модули, научиться взаимодействовать с user-space через `/proc` и `/sys`, создать базовый character device.  

**Окружение:**
- Виртуальная машина: VirtualBox  
- ОС: Ubuntu 24.04.3 LTS  
- Версия ядра: 6.14.0-34-generic  

---

## Задание A: Hello World модуль

### Код модуля

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameters");
MODULE_VERSION("1.0");

// Максимальная длина сообщения
#define MAX_MESSAGE_LEN 128

// Параметр модуля: строка message
static char message[MAX_MESSAGE_LEN] = "default message";

// Функция для валидации параметра
static int message_param_set(const char *val, const struct kernel_param *kp)
{
    size_t len;
    
    if (!val)
        return -EINVAL;
    
    len = strlen(val);
    if (len >= MAX_MESSAGE_LEN) {
        printk(KERN_WARNING "hello_module: Message too long (%zu), truncating to %d\n", 
               len, MAX_MESSAGE_LEN - 1);
        strncpy(message, val, MAX_MESSAGE_LEN - 1);
        message[MAX_MESSAGE_LEN - 1] = '\0';
        return 0;
    }
    
    strcpy(message, val);
    return 0;
}

static const struct kernel_param_ops message_param_ops = {
    .set = message_param_set,
    .get = param_get_string,
};

module_param_cb(message, &message_param_ops, &message, 0644);
MODULE_PARM_DESC(message, "Custom message to display (max 127 chars)");

// Функция инициализации модуля
static int __init hello_init(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Hello from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Message from module: %s\n", message);
    }
    return 0;
}

// Функция выгрузки модуля
static void __exit hello_exit(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Goodbye from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Goodbye! Last message was: %s\n", message);
    }
}

// Регистрация функций
module_init(hello_init);
module_exit(hello_exit);```

### Компиляция и загрузка

```bash
make
sudo insmod hello_module.ko
```

### Результаты работы

**Тест 1: Загрузка с параметром по умолчанию**

```bash
sudo insmod hello_module.ko
sudo dmesg | tail -5
```

**Вывод dmesg:**
```
[ 1317.202540]  ret_from_fork_asm+0x1a/0x30
[ 1317.202547]  </TASK>
[ 3018.371184] hello_module: loading out-of-tree module taints kernel.
[ 3018.371191] hello_module: module verification failed: signature and/or required key missing - tainting kernel
[ 3018.372640] Hello from Ковалёв Иван module!
```

**Тест 2: Загрузка с кастомным сообщением**

```bash
sudo rmmod hello_module
sudo insmod hello_module.ko message='"Hello from kernel space!"'
dmesg | tail -5
```

**Вывод dmesg:**
```
[ 5019.131123] Custom   

[ 5661.362696] Hello from Ковалёв Иван module!
[ 5674.644410] Goodbye from Ковалёв Иван module!
[ 5717.560900] Message from module: Hello from kernel space!
```

**Тест 3: Выгрузка модуля**

```bash
sudo rmmod hello_module
dmesg | tail -5
```

**Вывод dmesg:**
```
[ 5661.362696] Hello from Ковалёв Иван module!
[ 5674.644410] Goodbye from Ковалёв Иван module!
[ 5717.560900] Message from module: Hello from kernel space!
[ 5732.548434] Goodbye! Last message was: Hello from kernel space!
```

### Объяснение работы

Модуль демонстрирует:
- **Базовую структуру модуля** с функциями `init` и `exit`  
- **Использование параметров** через `module_param`  
- **Логирование в ядре** с помощью `printk`  
- **Динамическую загрузку/выгрузку** без перезагрузки системы  

---

## Задание B: /proc файл с информацией

### Код модуля

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("/proc/student_info module with statistics");
MODULE_VERSION("1.0");

// Глобальные переменные
static unsigned long module_load_time;
static int read_count = 0;

// Для современных версий ядра используем proc_ops
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#define HAVE_PROC_OPS
#endif

// Функция чтения из /proc файла
static ssize_t student_info_read(struct file *file, char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    char buffer[512];
    int len;
    
    // Если уже прочитали весь файл, возвращаем 0
    if (*ppos > 0)
        return 0;
    
    // Увеличиваем счетчик обращений
    read_count++;
    
    // Формируем строку с информацией
    len = snprintf(buffer, sizeof(buffer),
                  "Name: Ковалёв Иван\n"
                  "Group: 9, Subgroup: 1\n"
                  "Module loaded at: %lu jiffies\n"
                  "Read count: %d\n\n",
                  module_load_time, read_count);
    
    // Копируем данные из kernel-space в user-space
    if (copy_to_user(user_buf, buffer, len)) {
        return -EFAULT;  // Ошибка копирования
    }
    
    *ppos = len;  // Обновляем позицию
    return len;    // Возвращаем количество прочитанных байт
}

// Определяем операции с файлом
#ifdef HAVE_PROC_OPS
static const struct proc_ops student_info_fops = {
    .proc_read = student_info_read,
};
#else
static const struct file_operations student_info_fops = {
    .read = student_info_read,
};
#endif

// Указатель на proc файл
static struct proc_dir_entry *proc_file;

// Функция инициализации модуля
static int __init proc_module_init(void)
{
    // Сохраняем время загрузки модуля
    module_load_time = jiffies;
    
    // Создаем файл в /proc
    proc_file = proc_create("student_info", 0444, NULL, &student_info_fops);
    
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc/student_info\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "proc_module: /proc/student_info created\n");
    printk(KERN_INFO "proc_module: Loaded at %lu jiffies\n", module_load_time);
    
    return 0;
}

// Функция выгрузки модуля
static void __exit proc_module_exit(void)
{
    // Удаляем proc файл
    if (proc_file)
        proc_remove(proc_file);
    
    printk(KERN_INFO "proc_module: /proc/student_info removed\n");
    printk(KERN_INFO "proc_module: Final read count: %d\n", read_count);
}

// Регистрация функций
module_init(proc_module_init);
module_exit(proc_module_exit);
```

### Компиляция и загрузка

```bash
make
sudo insmod proc_module.ko
```

### Результаты работы

**Тест 1: Создание /proc файла**

```bash
sudo insmod proc_module.ko
dmesg | tail -5
```

**Вывод dmesg:**
```
[ 7866.946829] /dev/sr0: Can't open blockdev
[ 7866.948322] ISO 9660 Extensions: Microsoft Joliet Level 3
[ 7866.949504] ISO 9660 Extensions: RRIP_1991A
[ 8083.151249] proc_module: /proc/student_info created
[ 8083.151253] proc_module: Loaded at 4302750358 jiffies
```

**Тест 2: Чтение файла первый раз**

```bash
cat /proc/student_info
```

**Вывод:**
```
Name: Ковалёв Иван
Group: 9, Subgroup: 1
Module loaded at: 4302750358 jiffies
Read count: 1
```

**Тест 3: Чтение файла второй раз**

```bash
cat /proc/student_info
```

**Вывод:**
```
Name: Ковалёв Иван
Group: 9, Subgroup: 1
Module loaded at: 4302750358 jiffies
Read count: 2
```

**Тест 4: Выгрузка модуля**

```bash
sudo rmmod proc_module
dmesg | tail -5
```

**Вывод dmesg:**
```
[ 8083.151249] proc_module: /proc/student_info created
[ 8083.151253] proc_module: Loaded at 4302750358 jiffies
[ 8209.611618] audit: type=1400 audit(1762513028.020:161): apparmor="DENIED" operation="open" class="file" profile="snap.firmware-updater.firmware-notifier" name="/proc/sys/vm/max_map_count" pid=13849 comm="firmware-notifi" requested_mask="r" denied_mask="r" fsuid=1000 ouid=0
[ 8323.589100] proc_module: /proc/student_info removed
[ 8323.589104] proc_module: Final read count: 2
```

### Объяснение работы

Модуль демонстрирует:  
- **Создание виртуального файла** в `/proc` файловой системе  
- **Взаимодействие с user-space** через `copy_to_user()`  
- **Отслеживание состояния** с помощью глобальных переменных  
- **Автоматическое увеличение счетчика** при каждом чтении файла  
- **Использование jiffies** для измерения времени загрузки модуля  

---

## Задание C: Простой character device

### Код модуля

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

// Максимальный размер буфера
#define DEVICE_BUFFER_SIZE 1024
#define DEVICE_NAME "mychardev"

// Структура для хранения состояния устройства
struct chardev_data {
    struct cdev cdev;
    char buffer[DEVICE_BUFFER_SIZE];
    size_t buffer_size;
    atomic_t open_count;
    struct mutex buffer_mutex;
};

static int major_number = 0;
static struct chardev_data *device_data = NULL;
static struct class *char_class = NULL;
static struct device *char_device = NULL;  // Добавляем указатель на устройство

// Прототипы функций file_operations
static int device_open(struct inode *inode, struct file *file);
static int device_release(struct inode *inode, struct file *file);
static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t count, loff_t *offset);
static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t count, loff_t *offset);

// Структура file_operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

// Функция открытия устройства
static int device_open(struct inode *inode, struct file *file)
{
    struct chardev_data *data = container_of(inode->i_cdev, struct chardev_data, cdev);
    
    file->private_data = data;
    
    atomic_inc(&data->open_count);
    printk(KERN_INFO "chardev: Device opened (open count: %d)\n", 
           atomic_read(&data->open_count));
    
    return 0;
}

// Функция закрытия устройства
static int device_release(struct inode *inode, struct file *file)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    
    if (data) {
        atomic_dec(&data->open_count);
        printk(KERN_INFO "chardev: Device closed (open count: %d)\n", 
               atomic_read(&data->open_count));
    }
    
    return 0;
}

// Функция чтения из устройства
static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t count, loff_t *offset)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    ssize_t bytes_to_read;
    
    if (mutex_lock_interruptible(&data->buffer_mutex))
        return -ERESTARTSYS;
    
    if (*offset >= data->buffer_size) {
        mutex_unlock(&data->buffer_mutex);
        return 0;
    }
    
    bytes_to_read = min((size_t)(data->buffer_size - *offset), count);
    
    if (copy_to_user(user_buf, data->buffer + *offset, bytes_to_read)) {
        mutex_unlock(&data->buffer_mutex);
        return -EFAULT;
    }
    
    *offset += bytes_to_read;
    
    mutex_unlock(&data->buffer_mutex);
    
    printk(KERN_DEBUG "chardev: Read %zd bytes from device\n", bytes_to_read);
    return bytes_to_read;
}

// Функция записи в устройство
static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t count, loff_t *offset)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    ssize_t bytes_to_write;
    
    if (mutex_lock_interruptible(&data->buffer_mutex))
        return -ERESTARTSYS;
    
    bytes_to_write = min(count, (size_t)DEVICE_BUFFER_SIZE);
    
    if (copy_from_user(data->buffer, user_buf, bytes_to_write)) {
        mutex_unlock(&data->buffer_mutex);
        return -EFAULT;
    }
    
    data->buffer_size = bytes_to_write;
    *offset = bytes_to_write;
    
    mutex_unlock(&data->buffer_mutex);
    
    printk(KERN_DEBUG "chardev: Written %zd bytes to device\n", bytes_to_write);
    
    return bytes_to_write;
}

// Функция инициализации модуля
static int __init char_device_init(void)
{
    dev_t dev_num;
    int ret;
    
    printk(KERN_INFO "chardev: Initializing character device\n");
    
    // Выделяем память для данных устройства
    device_data = kzalloc(sizeof(struct chardev_data), GFP_KERNEL);
    if (!device_data) {
        printk(KERN_ERR "chardev: Failed to allocate device data\n");
        return -ENOMEM;
    }
    
    // Получаем динамический major number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to allocate device number\n");
        kfree(device_data);
        return ret;
    }
    
    major_number = MAJOR(dev_num);
    
    // Инициализируем структуру cdev
    cdev_init(&device_data->cdev, &fops);
    device_data->cdev.owner = THIS_MODULE;
    
    // Добавляем character device в систему
    ret = cdev_add(&device_data->cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to add character device\n");
        goto error_cdev_add;
    }
    
    // Создаем класс устройства
    char_class = class_create("chardev_class");
    if (IS_ERR(char_class)) {
        ret = PTR_ERR(char_class);
        printk(KERN_ERR "chardev: Failed to create device class: %d\n", ret);
        goto error_class_create;
    }
    
    // Создаем устройство в /dev и проверяем результат
    char_device = device_create(char_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(char_device)) {
        ret = PTR_ERR(char_device);
        printk(KERN_ERR "chardev: Failed to create device: %d\n", ret);
        goto error_device_create;
    }
    
    // Инициализируем данные устройства
    device_data->buffer_size = 0;
    atomic_set(&device_data->open_count, 0);
    mutex_init(&device_data->buffer_mutex);
    memset(device_data->buffer, 0, DEVICE_BUFFER_SIZE);
    
    printk(KERN_INFO "chardev: Character device registered successfully\n");
    printk(KERN_INFO "chardev: Major number = %d\n", major_number);
    printk(KERN_INFO "chardev: Device created: /dev/%s\n", DEVICE_NAME);
    
    return 0;

// Обработка ошибок с правильной последовательностью очистки
error_device_create:
    class_destroy(char_class);
error_class_create:
    cdev_del(&device_data->cdev);
error_cdev_add:
    unregister_chrdev_region(dev_num, 1);
    kfree(device_data);
    return ret;
}

// Функция выгрузки модуля
static void __exit char_device_exit(void)
{
    dev_t dev_num = MKDEV(major_number, 0);
    
    printk(KERN_INFO "chardev: Unloading character device\n");
    
    // Удаляем устройство из /dev (только если оно было создано)
    if (!IS_ERR_OR_NULL(char_device)) {
        device_destroy(char_class, dev_num);
    }
    
    // Удаляем класс (только если он был создан)
    if (!IS_ERR_OR_NULL(char_class)) {
        class_destroy(char_class);
    }
    
    // Удаляем character device
    if (device_data) {
        cdev_del(&device_data->cdev);
    }
    
    // Освобождаем device number
    unregister_chrdev_region(dev_num, 1);
    
    // Освобождаем память
    if (device_data) {
        kfree(device_data);
    }
    
    printk(KERN_INFO "chardev: Character device unregistered\n");
}

// Регистрация функций
module_init(char_device_init);
module_exit(char_device_exit);```

### Компиляция и загрузка

```bash
make
sudo insmod char_device.ko
```

### Результаты работы

**Тест 1: Инициализация устройства**

```bash
sudo insmod char_device.ko
dmesg | tail -5
```

**Вывод dmesg:**
```
[12250.246775] chardev: Character device unregistered
[12270.762390] chardev: Initializing character device
[12270.762569] chardev: Character device registered successfully
[12270.762571] chardev: Major number = 240
[12270.762574] chardev: Use: sudo mknod /dev/mychardev c 240 0
```

**Тест 2: Запись данных в устройство**

```bash
echo "Test message from user" > /dev/mychardev
dmesg | tail -5
```

**Вывод dmesg:**
```
[12340.777325] chardev: Device opened (open count: 1)
[12340.777347] chardev: Written 23 bytes to device
[12340.777349] chardev: Data: Test message from user

[12340.777354] chardev: Device closed (open count: 0)
```

**Тест 3: Чтение данных из устройства**

```bash
cat /dev/mychardev
dmesg | tail -5
```

**Вывод:**
```
Test message from user
```

**Вывод dmesg:**
```

[12340.777354] chardev: Device closed (open count: 0)
[12353.358541] chardev: Device opened (open count: 1)
[12353.358568] chardev: Read 23 bytes from device
[12353.358590] chardev: Device closed (open count: 0)
```

**Тест 5: Выгрузка модуля**

```bash

sudo rmmod char_device
dmesg | tail -3
```

**Вывод dmesg:**
```
[12353.358541] chardev: Device opened (open count: 1)
[12353.358568] chardev: Read 23 bytes from device
[12353.358590] chardev: Device closed (open count: 0)
[12567.850206] chardev: Unloading character device
[12567.850427] chardev: Character device unregistered
```

### Объяснение работы

Модуль демонстрирует:
- **Создание character device** с динамическим major number  
- **Регистрацию операций** через `file_operations`  
- **Безопасное копирование данных** между kernel-space и user-space  
- **Управление состоянием устройства** через структуру `chardev_data`  
- **Отслеживание открытий/закрытий** устройства  
- **Буферизацию данных** с ограничением размера  

---

## Ответы на вопросы

### Базовые понятия:
1. **Что такое модуль ядра и зачем он нужен?**  
   Модуль ядра - это фрагмент кода, который может быть динамически загружен и выгружен из ядра Linux. Он нужен для расширения функциональности ядра без перекомпиляции и перезагрузки.  

2. **Чем отличается kernel-space от user-space?**  
   Kernel-space работает в привилегированном режиме с полным доступом к аппаратуре, а user-space - в защищенном режиме с ограниченным доступом.  

3. **Что произойдёт, если в модуле обратиться к NULL указателю?**  
   Произойдет kernel panic - система аварийно завершит работу.  

4. **Почему нельзя использовать `printf()` в модуле ядра?**  
   В kernel-space нет стандартной библиотеки C, вместо `printf()` используется `printk()`.  

5. **Что такое kernel panic и как его избежать?**  
   Kernel panic - критическая ошибка ядра. Чтобы избежать, нужно проверять указатели, возвращаемые значения и использовать правильные функции копирования.  

### Жизненный цикл модуля:
6. **Какие функции вызываются при `insmod` и `rmmod`?**  
   При `insmod` вызывается `module_init()`, при `rmmod` - `module_exit()`.  

7. **Что должна делать функция `module_exit()`?**  
   Освобождать все ресурсы, выделенные в `module_init()`.  

8. **Что происходит, если `module_init()` возвращает ошибку?**  
   Модуль не загружается, и `module_exit()` не вызывается.  

9. **Можно ли выгрузить модуль, если он используется?**  
   Нет, сначала нужно закрыть все процессы, использующие модуль.  

### Логирование и отладка:
10. **Чем `printk()` отличается от `printf()`?**  
    `printk()` выводит в kernel log buffer и имеет уровни важности, а `printf()` - в stdout.  

11. **Какие уровни логирования существуют в ядре?**  
    KERN_EMERG   "System is unusable"           // 0 - Аварийная  
    KERN_ALERT   "Action must be taken"         // 1 - Требует действий  
    KERN_CRIT    "Critical conditions"          // 2 - Критическая  
    KERN_ERR     "Error conditions"             // 3 - Ошибка  
    KERN_WARNING "Warning conditions"           // 4 - Предупреждение  
    KERN_NOTICE  "Normal but significant"       // 5 - Заметка  
    KERN_INFO    "Informational"                // 6 - Информация  
    KERN_DEBUG   "Debug-level messages"         // 7 - Отладка  

12. **Как посмотреть логи модуля?**  
    С помощью `dmesg` или `journalctl -k`.  

13. **Что означает "tainted kernel"?**  
    Ядро, загрузившее проприетарные модули или модули без правильной лицензии.  

### Память:
14. **Чем `kmalloc()` отличается от `malloc()`?**  
    `kmalloc()` выделяет физически непрерывную память в ядре, `malloc()` - виртуальную в user-space.  

15. **Что такое флаги GFP и зачем они нужны?**  
    Флаги Get Free Pages определяют условия выделения памяти (GFP_KERNEL, GFP_ATOMIC и т.д.).  

16. **Что произойдёт, если не освободить память в `module_exit()`?**  
    Произойдет утечка памяти, которая не восстановится до перезагрузки.  

17. **Почему нельзя использовать user-space указатели напрямую в ядре?**  
    Потому что user-space и kernel-space используют разные адресные пространства.  

### Взаимодействие с user-space:
18. **Что такое `/proc` и для чего он используется?**  
    Виртуальная файловая система для экспорта информации из ядра.  

19. **Что такое `/sys` (sysfs) и чем отличается от procfs?**  
    Sysfs - более структурированная система для атрибутов устройств, procfs - для общей информации.  

20. **Зачем нужны функции `copy_to_user()` и `copy_from_user()`?**  
    Для безопасного копирования данных между kernel-space и user-space.  

21. **Что такое character device и как он работает?**  
    Устройство, с которым можно работать как с файлом, реализуя операции read/write.  

### Параметры и метаданные:
22. **Как передать параметры модулю при загрузке?**  
    Через `insmod module.ko param=value`.  

23. **Зачем нужен `MODULE_LICENSE()`?**  
    Для указания лицензии модуля, без него ядро будет "tainted".  

24. **Что произойдёт, если не указать лицензию?**  
    Модуль будет работать, но ядро пометится как "tainted".  

### Безопасность:
25. **Какие основные правила безопасного кода в ядре?**  
    Проверять возвращаемые значения, освобождать ресурсы, использовать правильные функции копирования.  

26. **Можно ли использовать бесконечный цикл в модуле?**  
    Нет, это может заблокировать систему.  

27. **Почему в ядре нет FPU операций?**  
    FPU операции требуют сохранения состояния и могут быть прерваны.  

28. **Что делать, если модуль вызвал kernel panic?**  
    Перезагрузить систему и исправить код модуля.  

### Практические вопросы:
29. **Как узнать, какие модули загружены в системе?**  
    Команда `lsmod` или `cat /proc/modules`.  

30. **Как получить информацию о модуле (версия, параметры)?**  
    Команда `modinfo module_name`.  

---

## Заключение

1. Успешно изучена архитектура модулей ядра Linux  
2. Реализованы три типа модулей: простой Hello World, /proc интерфейс и character device  
3. Освоено взаимодействие между kernel-space и user-space  
4. Изучены особенности работы с памятью в ядре  
5. Приобретены навыки отладки модулей ядра  

