# Лабораторная 5 — Модули ядра Linux

Практика по разработке модулей ядра Linux: изучим архитектуру ядра, напишем простые модули, научимся взаимодействовать с user-space через `/proc` и `/sys`, создадим базовый character device.

## ⚠️ ВАЖНОЕ ПРЕДУПРЕЖДЕНИЕ О БЕЗОПАСНОСТИ

**Эта лабораторная работа ОБЯЗАТЕЛЬНО выполняется в виртуальной машине или изолированной среде!**

### Требования к окружению:
Использовал **VirtualBox/VMware** с Ubuntu 22.04/24.04

## Задания

### Вариант
я 17 в списке (нечётный номер) → Вариант 1

### Задание A: Hello World модуль

Создайте простой модуль, который:
1. При загрузке (`insmod`) выводит "Hello from [ВАШ_ИМЯ] module!"
2. При выгрузке (`rmmod`) выводит "Goodbye from [ВАШ_ИМЯ] module!"
3. Принимает параметр `message` (строка)
4. Если параметр задан, выводит его вместо дефолтного сообщения

**Требования:**
- Использовать `printk` с `KERN_INFO`
- Правильные метаданные (`MODULE_LICENSE`, `MODULE_AUTHOR`)
- `module_param` для параметра

Исходный код:


```bash
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = "Hello from Vanya's module!";
static char *byeMessage = "Goodbye from Vanya's module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message");

static int __init hello_init(void)
{
printk(KERN_INFO "[hello_module] %s\n", message);
return 0;
}

static void __exit hello_exit(void)
{
printk(KERN_INFO "[hello_module] %s\n", byeMessage);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Hello World Kernel Module");
```
Результаты проверки видно в приложенных скриншотах в папке screenshots

---

### Задание B: /proc файл с информацией

Создайте модуль, который создаёт файл `/proc/student_info` с информацией:
- Ваше имя
- Группа и подгруппа
- Текущее время загрузки модуля (в секундах с boot, используйте `jiffies`)
- Счётчик обращений к файлу

**Требования:**
- Использовать `proc_create()` и `proc_remove()`
- Реализовать функцию чтения (`proc_read`)
- Счётчик должен увеличиваться при каждом `cat /proc/student_info`

**Исходный код:**
```
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>

#define PROC_NAME "student_info"

static struct proc_dir_entry *proc_file;
static unsigned long load_time_jiffies;
static int read_count = 0;

static ssize_t student_info_read(struct file *file, char __user *buf,
                                 size_t count, loff_t *ppos)
{
    char info[256];
    int len;

    if (*ppos > 0)
        return 0;

    read_count++;

    len = snprintf(info, sizeof(info),
                   "Name: Filon Ivan\n"
                   "Group: 1, Subgroup: 1\n"
                   "Module loaded at: %lu jiffies\n"
                   "Read count: %d\n",
                   load_time_jiffies, read_count);

    if (copy_to_user(buf, info, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

static const struct proc_ops student_proc_ops = {
    .proc_read = student_info_read,
};

static int __init student_module_init(void)
{
    load_time_jiffies = jiffies;

    proc_file = proc_create(PROC_NAME, 0444, NULL, &student_proc_ops);
    if (!proc_file)
        return -ENOMEM;

    printk(KERN_INFO "[student_module] /proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit student_module_exit(void)
{
    proc_remove(proc_file);
    printk(KERN_INFO "[student_module] /proc/%s removed\n", PROC_NAME);
}

module_init(student_module_init);
module_exit(student_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Module creating /proc/student_info");
```
Результат первого и повторного чтения файла можете увидеть на скриншотах, значение count действительно меняется

---

### Задание C: Простой character device

Создайте character device `/dev/mychardev`, который:
1. Можно открыть/закрыть
2. При записи сохраняет данные в kernel buffer (максимум 1024 байта)
3. При чтении возвращает сохранённые данные
4. Выводит в `dmesg` когда устройство открывается/закрывается

**Требования:**
- Использовать `alloc_chrdev_region()`, `cdev_init()`, `cdev_add()`
- Реализовать `open`, `release`, `read`, `write`
- Использовать `copy_to_user()` и `copy_from_user()`

**Проверка:**
```bash
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static char *kernel_buffer;

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "[mychardev] Device opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "[mychardev] Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    size_t remaining;
    size_t to_copy;

    if (*off >= BUFFER_SIZE) 
        return 0;

    remaining = BUFFER_SIZE - *off;
    to_copy = len < remaining ? len : remaining;

    if (copy_to_user(buf, kernel_buffer + *off, to_copy))
        return -EFAULT;

    *off += to_copy;
    return to_copy;
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
    size_t to_copy = len < BUFFER_SIZE ? len : BUFFER_SIZE;

    if (copy_from_user(kernel_buffer, buf, to_copy))
        return -EFAULT;

    return to_copy;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init chardev_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
        return -1;

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }

    memset(kernel_buffer, 0, BUFFER_SIZE);
    printk(KERN_INFO "[mychardev] Device registered with major %d\n", MAJOR(dev_num));
    return 0;
}

static void __exit chardev_exit(void)
{
    kfree(kernel_buffer);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "[mychardev] Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Simple character device /dev/mychardev");
```
Аналогично предыдущим заданиям, проверка работоспособности и результаты видны на приложенных скриншотах

---

## Вопросы для отчёта (обязательно)

### Базовые понятия:
1. Что такое модуль ядра и зачем он нужен?
   Модуль ядра — загружаемый в ядро бинарный объект, расширяющий функциональность без пересборки или перезагрузки ядра.
2. Чем отличается kernel-space от user-space?
   Это разные адресные пространства, ядро имеет полный доступ к ресурсам, пользовательский же режим ограничен
3. Что произойдёт, если в модуле обратиться к NULL указателю?
   Нет защиты страниц, так что kernel panic
4. Почему нельзя использовать `printf()` в модуле ядра?
   Потому что в ядре нет libc
5. Что такое kernel panic и как его избежать?
    Это критическая ошибка ядра, которая приводит к остановке системы, для предотвращения проверяют указатели, аккуратно работают с памятью

### Жизненный цикл модуля:
6. Какие функции вызываются при `insmod` и `rmmod`?
   При insmod вызывается функция из module_init(),  при rmmod вызывается из module_exit()
7. Что должна делать функция `module_exit()`?
   Должна освобождать все ресурсы
8. Что происходит, если `module_init()` возвращает ошибку?
   Модуль не загрузится, вызовется откат
9. Можно ли выгрузить модуль, если он используется?
   Нет, это невозможно, необходимо закрыть дескрипторы/освободить пользователей

### Логирование и отладка:
10. Чем `printk()` отличается от `printf()`?
    printk() работает в ядре, поддерживает уровни по типу KERN_INFO, пишет в dmesg, printf часть libc в user-space
11. Какие уровни логирования существуют в ядре?
    KERN_EMERG, DEBUG, CONT, ERR, WARN, NOTICE, INFO, ALERT, CRIT
12. Как посмотреть логи модуля?
    dmesg, журналы
13. Что означает "tainted kernel"?
    флаг загрязнения ядра, например, ошибки oops

### Память:
14. Чем `kmalloc()` отличается от `malloc()`?
    kmalloc - аллокатор ядра, malloc - аллокатор из libc
15. Что такое флаги GFP и зачем они нужны?
    Политика выделения памяти, ограничения 
16. Что произойдёт, если не освободить память в `module_exit()`?
    Утечка в ядре, нестабильность
17. Почему нельзя использовать user-space указатели напрямую в ядре?
    Так как нужна проверка через copy_from_user() из-за защиты памяти и разного адресного пространства

### Взаимодействие с user-space:
18. Что такое `/proc` и для чего он используется?
    Виртуальная ФС для изучения состояния ядра или процессов и обмена информацией с user-space
19. Что такое `/sys` (sysfs) и чем отличается от procfs?
    Иерархия объектов ядра/девайсов/драйверов, более строгая модель, а procf отвечает чисто за состояние и статистику
20. Зачем нужны функции `copy_to_user()` и `copy_from_user()`?
    Для безопасного копирования между адресными пространствами ядра и пользователя
21. Что такое character device и как он работает?
    байтово-ориентированный интерфейс, реализованный драйвером через file_operations/cdev

### Параметры и метаданные:
22. Как передать параметры модулю при загрузке?
    При помощи приписки после вызова insmod: insmod module.ko param=value
23. Зачем нужен `MODULE_LICENSE()`?
    Для сообщения ядру лицензии, то есть влияет на доступ к некоторым символам и пометку taint
24. Что произойдёт, если не указать лицензию?
    Модуль будет помечен как non-GPL, доступ к символам GPL закрыт

### Безопасность:
25. Какие основные правила безопасного кода в ядре?
    Проверка всех указателей/возвратов, корректная синхронизация, отсутствие гонок/утечек, минимизация областей с отключенными прерываниями
26. Можно ли использовать бесконечный цикл в модуле?
    Только в корректном контексте(таймеры/потоки) и с точками выхода, зависание в методах init и exit недопустимл
27. Почему в ядре нет FPU операций?
    Дорогое сохранения, восстановление контекста, исторические ограничения
28. Что делать, если модуль вызвал kernel panic?
    По возможности выгрузить модуль, перезагрузить, проанализировать стек, исправить, протестировать

### Практические вопросы:
29. Как узнать, какие модули загружены в системе?
    С помощью lsmod, cat /proc/modules
30. Как получить информацию о модуле (версия, параметры)?
    modinfo module.ko

---

## Как использовался ИИ
Для настройки окржуения
Для написания Makefile
Для разрешения ошибок в ходе сборки модулей, их тестирования