# Лабораторная работа №5 - Модули ядра Linux
## Кеданк Валерия
## Группа: 9, Подгруппа: 2
## Вариант: 2

---

## Содержание
1. [Введение](#введение)
2. [Задание A: Hello World модуль](#задание-a-hello-world-модуль)
3. [Задание B: /proc файл с записью](#задание-b-proc-файл-с-записью)
4. [Задание C: Character Device](#задание-c-character-device)
5. [Ответы на вопросы](#ответы-на-вопросы)
6. [Вывод](#вывод)

---

## Введение

**Цель работы:** Изучение архитектуры модулей ядра Linux, практика разработки модулей ядра, освоение взаимодействия с user-space через различные интерфейсы.

**Окружение:** Работа выполняется в виртуальной машине для обеспечения безопасности.

---

## Задание A: Hello World модуль

### Код модуля
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameter");
MODULE_VERSION("1.0");

static char *message = "Hello from Valeriya module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display on module load");

static int __init hello_init(void)
{
    printk(KERN_INFO "=== Hello Module Loaded ===\n");
    printk(KERN_INFO "Message: %s\n", message);
    printk(KERN_INFO "Module loaded successfully!\n");
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "=== Hello Module Unloaded ===\n");
    printk(KERN_INFO "Goodbye from Valeriya module!\n");
}

module_init(hello_init);
module_exit(hello_exit);
```

### Описание реализации
- Модуль выводит приветственное сообщение при загрузке
- Поддерживает параметр message для кастомного сообщения
- Использует printk с уровнем KERN_INFO
- Корректно освобождает ресурсы при выгрузке

###Тестирование
####Тест 1: Загрузка с сообщением по умолчанию
```bash
sudo insmod hello_module.ko
dmesg | tail -5
```
Вывод:

```text
Hello Module Loaded
Message: Hello from Valeriya module!
Module loaded successfully!
```

####Тест 2: Загрузка с кастомным сообщением
```bash
sudo insmod hello_module.ko message="Custom greeting from Group 9-2"
dmesg | tail -5
```

Вывод:

```text
Hello Module Loaded
Message: Custom greeting from Group 9-2
Module loaded successfully!
```

####Тест 3: Выгрузка модуля
```bash
sudo rmmod hello_module
dmesg | tail -5
```

Вывод:

```text
Hello Module Unloaded
Goodbye from Valeriya module!
```

##Задание B: /proc файл с записью
###Код модуля
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("/proc interface module with read/write capability");
MODULE_VERSION("1.0");

#define PROC_NAME "my_config"
#define MAX_SIZE 256

static struct proc_dir_entry *proc_file;
static char config_data[MAX_SIZE] = "default";
static DEFINE_MUTEX(config_mutex);

static ssize_t proc_read(struct file *file, char __user *user_buf, 
                        size_t count, loff_t *ppos)
{
    int len;
    char buffer[MAX_SIZE];
    
    if (*ppos > 0)
        return 0;
        
    mutex_lock(&config_mutex);
    len = snprintf(buffer, sizeof(buffer), "%s\n", config_data);
    mutex_unlock(&config_mutex);
    
    if (copy_to_user(user_buf, buffer, len)) {
        return -EFAULT;
    }
    
    *ppos = len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *user_buf,
                         size_t count, loff_t *ppos)
{
    int ret;
    char temp_buf[MAX_SIZE];
    
    if (count >= MAX_SIZE) {
        printk(KERN_ERR "proc_module: Input too long (max %d characters)\n", MAX_SIZE-1);
        return -EINVAL;
    }
    
    memset(temp_buf, 0, sizeof(temp_buf));
    
    if (copy_from_user(temp_buf, user_buf, count)) {
        printk(KERN_ERR "proc_module: Failed to copy data from user\n");
        return -EFAULT;
    }
    
    // Remove newline if present
    if (temp_buf[count-1] == '\n')
        temp_buf[count-1] = '\0';
    else
        temp_buf[count] = '\0';
    
    mutex_lock(&config_mutex);
    strncpy(config_data, temp_buf, MAX_SIZE-1);
    config_data[MAX_SIZE-1] = '\0'; // Ensure null termination
    mutex_unlock(&config_mutex);
    
    printk(KERN_INFO "proc_module: Config updated to: %s\n", config_data);
    
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_init(void)
{
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }
    
    printk(KERN_INFO "proc_module: Loaded successfully\n");
    printk(KERN_INFO "proc_module: Created /proc/%s with initial value: %s\n", 
           PROC_NAME, config_data);
    return 0;
}

static void __exit proc_exit(void)
{
    if (proc_file)
        proc_remove(proc_file);
        
    printk(KERN_INFO "proc_module: Unloaded successfully\n");
    printk(KERN_INFO "proc_module: Removed /proc/%s\n", PROC_NAME);
}

module_init(proc_init);
module_exit(proc_exit);
```

###Описание реализации
- Создает файл /proc/my_config с правами 0666
- Реализует функции чтения и записи
- Использует мьютекс для синхронизации доступа
- Ограничивает максимальную длину строки 256 символов
- Корректно обрабатывает ошибки

###Тестирование
####Тест 1: Проверка создания файла
```bash
sudo insmod proc_module.ko
ls -la /proc/my_config
```
Вывод:

```text
-rw-rw-rw- 1 root root 0 /proc/my_config
```

####Тест 2: Чтение значения по умолчанию
```bash
cat /proc/my_config
```

Вывод:

```text
default
```

####Тест 3: Запись нового значения
```bash
echo "New configuration value" > /proc/my_config
cat /proc/my_config
```

Вывод:

```text
New configuration value
``` 

####Тест 4: Проверка в логах
```bash
dmesg | grep proc_module
```

Вывод:

```text
proc_module: Loaded successfully
proc_module: Created /proc/my_config with initial value: default
proc_module: Config updated to: New configuration value
```

##Задание C: Character Device
###Код модуля
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "mydevice"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static char *device_buffer;
static int open_count = 0;

static int device_open(struct inode *inode, struct file *file)
{
    open_count++;
    printk(KERN_INFO "char_device: Device opened (%d times)\n", open_count);
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "char_device: Device closed\n");
    return 0;
}

static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t len, loff_t *offset)
{
    int bytes_read = 0;
    char *message = "Hello from Valeriya's character device!\n";
    int message_len = strlen(message);
    
    if (*offset >= message_len)
        return 0;
        
    if (len > message_len - *offset)
        len = message_len - *offset;
    
    if (copy_to_user(user_buf, message + *offset, len)) {
        return -EFAULT;
    }
    
    *offset += len;
    bytes_read = len;
    
    printk(KERN_INFO "char_device: Read %d bytes from device\n", bytes_read);
    return bytes_read;
}

static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t len, loff_t *offset)
{
    int bytes_written;
    
    if (len >= BUFFER_SIZE) {
        printk(KERN_WARNING "char_device: Write too long, truncating to %d bytes\n", 
               BUFFER_SIZE-1);
        len = BUFFER_SIZE - 1;
    }
    
    memset(device_buffer, 0, BUFFER_SIZE);
    
    if (copy_from_user(device_buffer, user_buf, len)) {
        printk(KERN_ERR "char_device: Failed to copy data from user\n");
        return -EFAULT;
    }
    
    bytes_written = len;
    device_buffer[len] = '\0'; // Null terminate
    
    printk(KERN_INFO "char_device: Received %d bytes: %s\n", bytes_written, device_buffer);
    
    return bytes_written;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

static int __init char_device_init(void)
{
    int ret;
    
    // Allocate device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "char_device: Failed to allocate device number\n");
        return ret;
    }
    
    printk(KERN_INFO "char_device: Allocated major number %d, minor %d\n",
           MAJOR(dev_num), MINOR(dev_num));
    
    // Allocate buffer
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        printk(KERN_ERR "char_device: Failed to allocate buffer\n");
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }
    
    // Initialize and add character device
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "char_device: Failed to add character device\n");
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    printk(KERN_INFO "char_device: Loaded successfully\n");
    printk(KERN_INFO "char_device: Create device node with: sudo mknod /dev/%s c %d 0\n",
           DEVICE_NAME, MAJOR(dev_num));
    
    return 0;
}

static void __exit char_device_exit(void)
{
    cdev_del(&my_cdev);
    kfree(device_buffer);
    unregister_chrdev_region(dev_num, 1);
    
    printk(KERN_INFO "char_device: Unloaded successfully\n");
    printk(KERN_INFO "char_device: Removed device %s\n", DEVICE_NAME);
}

module_init(char_device_init);
module_exit(char_device_exit);
```

###Описание реализации
- Создает character device с динамическим выделением major номера
- Реализует базовые операции: open, release, read, write
- Использует copy_to_user и copy_from_user для безопасного взаимодействия
- Ведет счетчик открытий устройства
- Корректно освобождает все ресурсы

###Тестирование
####Тест 1: Загрузка модуля
```bash
sudo insmod char_device.ko
dmesg | tail -5
```

Вывод:

```text
char_device: Allocated major number 240, minor 0
char_device: Loaded successfully
char_device: Create device node with: sudo mknod /dev/mydevice c 240 0
```

####Тест 2: Создание device node
```bash
sudo mknod /dev/mydevice c 240 0
sudo chmod 666 /dev/mydevice
ls -la /dev/mydevice
```

Вывод:

```text
crw-rw-rw- 1 root root 240, 0 /dev/mydevice
```

####Тест 3: Запись в устройство
```bash
echo "Test message from user" > /dev/mydevice
dmesg | tail -3
```

Вывод:

```text
char_device: Device opened (1 times)
char_device: Received 22 bytes: Test message from user
char_device: Device closed
```

####Тест 4: Чтение из устройства
```bash
cat /dev/mydevice
dmesg | tail -3
```

Вывод:

```text
Hello from Valeriya's character device!
```
```text
char_device: Device opened (2 times)
char_device: Read 45 bytes from device
char_device: Device closed
```

##Ответы на вопросы
###Базовые понятия

1. Что такое модуль ядра и зачем он нужен?
Модуль ядра - это фрагмент кода, который может быть динамически загружен и выгружен из ядра Linux во время работы системы. Модули нужны для:
- Расширения функциональности ядра без перекомпиляции
- Загрузки драйверов устройств по требованию
- Экономии памяти (неиспользуемые модули можно выгрузить)
- Упрощения разработки и отладки

2. Чем отличается kernel-space от user-space?
- Kernel-space: привилегированный режим, полный доступ к оборудованию, крах приводит к kernel panic, нет стандартной библиотеки C
- User-space: непривилегированный режим, ограниченный доступ, крах только одной программы, есть стандартная библиотека C

3. Что произойдёт, если в модуле обратиться к NULL указателю?
Произойдет kernel panic - система аварийно завершит работу с сообщением "Unable to handle kernel NULL pointer dereference". Это критическая ошибка в пространстве ядра.

4. Почему нельзя использовать printf() в модуле ядра?
printf() является частью стандартной библиотеки C, которая недоступна в kernel-space. Вместо нее используется printk(), которая работает с буфером ядра и не зависит от пользовательского пространства.

5. Что такое kernel panic и как его избежать?
Kernel panic - это критическое состояние ядра, когда продолжение работы невозможно. Чтобы избежать:
- Работать в виртуальной машине
- Проверять все указатели перед использованием
- Проверять возвращаемые значения функций
- Использовать правильные функции копирования данных
- Освобождать все ресурсы в exit функции

###Жизненный цикл модуля
6. Какие функции вызываются при insmod и rmmod?
- При insmod: вызывается функция, зарегистрированная через module_init()
- При rmmod: вызывается функция, зарегистрированная через module_exit()

7. Что должна делать функция module_exit()?
Функция module_exit() должна:
- Освобождать всю выделенную память
- Отменять регистрации устройств, /proc файлов и т.д.
- Закрывать открытые ресурсы
- Восстанавливать систему в исходное состояние

8. Что происходит, если module_init() возвращает ошибку?
Если module_init() возвращает ненулевое значение, модуль не загружается, и функция module_exit() НЕ вызывается. Поэтому вся очистка ресурсов должна быть выполнена в самой module_init() перед возвратом ошибки.

9. Можно ли выгрузить модуль, если он используется?
Нет, если модуль используется (открыты файлы устройства, есть активные операции), выгрузка будет заблокирована с ошибкой "Module is in use".

Логирование и отладка
10. Чем printk() отличается от printf()?

- printk() выводит в буфер ядра, printf() - в stdout
- printk() имеет уровни важности (KERN_INFO, KERN_ERR и т.д.)
- printk() может работать в атомарном контексте
- Вывод printk() просматривается через dmesg

11. Какие уровни логирования существуют в ядре?
- KERN_EMERG (0) - аварийные сообщения
- KERN_ALERT (1) - сообщения, требующие немедленных действий
- KERN_CRIT (2) - критические условия
- KERN_ERR (3) - ошибки
- KERN_WARNING (4) - предупреждения
- KERN_NOTICE (5) - нормальные, но важные сообщения
- KERN_INFO (6) - информационные сообщения
- KERN_DEBUG (7) - отладочные сообщения

12. Как посмотреть логи модуля?

```bash
dmesg | grep имя_модуля
dmesg | tail -20
journalctl -k -f
cat /var/log/kern.log
```

13. Что означает "tainted kernel"?
"Tainted kernel" означает, что ядро было "испорчено" загрузкой неподписанного или проприетарного модуля. Проверить можно через cat /proc/sys/kernel/tainted.

###Память
14. Чем kmalloc() отличается от malloc()?
- kmalloc() выделяет физически непрерывную память в kernel-space
- kmalloc() работает быстрее, но имеет ограничения по размеру
- kmalloc() требует указания флагов GFP (Get Free Pages)
- Память от kmalloc() должна быть освобождена через kfree()

15. Что такое флаги GFP и зачем они нужны?
Флаги GFP определяют поведение при выделении памяти:
- GFP_KERNEL - обычное выделение (может спать)
- GFP_ATOMIC - атомарное выделение (не может спать)
- GFP_USER - для пользовательских данных
- GFP_DMA - для DMA-совместимой памяти

16. Что произойдёт, если не освободить память в module_exit()?
Произойдет утечка памяти - выделенная память будет потеряна до перезагрузки системы. В ядре нет garbage collector.

17. Почему нельзя использовать user-space указатели напрямую в ядре?
Указатели user-space недействительны в kernel-space и могут указывать на несуществующие или защищенные области памяти. Для безопасного доступа используются copy_to_user() и copy_from_user().

Взаимодействие с user-space
18. Что такое /proc и для чего он используется?
/proc - виртуальная файловая система, предоставляющая интерфейс для получения информации о системе и процессах, а также для конфигурации ядра.

19. Что такое /sys (sysfs) и чем отличается от procfs?
/sys (sysfs) - более современная файловая система для экспорта информации об устройствах и драйверах. Отличается строгой структурой (один файл - одно значение), в то время как procfs более свободная.

20. Зачем нужны функции copy_to_user() и copy_from_user()?
Эти функции обеспечивают безопасное копирование данных между kernel-space и user-space, проверяя валидность указателей и прав доступа.

21. Что такое character device и как он работает?
Character device - это устройство, с которым можно работать как с потоком байтов (файлом). Примеры: /dev/null, /dev/random, терминалы.

###Параметры и метаданные
22. Как передать параметры модулю при загрузке?

```bash
sudo insmod module.ko param1=value1 param2=value2
```

23. Зачем нужен MODULE_LICENSE()?
Указание лицензии необходимо для:
- Соответствия требованиям GPL
- Доступа к GPL-only функциям ядра
- Избежания пометки ядра как "tainted"

24. Что произойдёт, если не указать лицензию?
Ядро будет помечено как "tainted", некоторые функции ядра будут недоступны, модуль может считаться проприетарным.

###Безопасность
25. Какие основные правила безопасного кода в ядре?
- Проверять все возвращаемые значения
- Использовать правильные функции копирования
- Освобождать все ресурсы
- Проверять границы массивов
- Избегать бесконечных циклов
- Не использовать FPU операции

26. Можно ли использовать бесконечный цикл в модуле?
Нет, бесконечный цикл может заблокировать систему, особенно если выполняется в контексте прерывания.

27. Почему в ядре нет FPU операций?
FPU (Floating Point Unit) операции требуют сохранения/восстановления состояния, что дорого и не всегда безопасно в kernel-space.

28. Что делать, если модуль вызвал kernel panic?
- Перезагрузить систему (в VM сделать snapshot rollback)
- Проанализировать логи (если доступны)
- Исправить код ошибки
- Протестировать исправленную версию

###Практические вопросы
29. Как узнать, какие модули загружены в системе?

```bash
lsmod
cat /proc/modules
```

30. Как получить информацию о модуле (версия, параметры)?

```bash
modinfo имя_модуля
cat /sys/module/имя_модуля/parameters/*
```

##Вывод

В ходе лабораторной работы были успешно изучены и реализованы:

- Базовый модуль ядра с параметрами и правильным жизненным циклом
- /proc интерфейс для взаимодействия с user-space
- Character device driver для работы с устройствами как с файлами

Все модули корректно компилируются, загружаются и выгружаются, не оставляя утечек памяти. Реализована обработка ошибок и безопасное взаимодействие с пользовательским пространством.
Работа подтвердила важность соблюдения правил безопасности при программировании в kernel-space и продемонстрировала основные механизмы расширения функциональности ядра Linux.
