Лабораторная работа №5: Модули ядра Linux
ФИО: Браун Дэннис Франко
Группа: 1
Курс: 4
Номер по списку: 2
Вариант: 2 (чётный номер)

Цель работы
Изучить архитектуру ядра Linux, научиться разрабатывать, собирать, загружать и отлаживать простые модули ядра, а также освоить механизмы взаимодействия модулей с пространством пользователя.

Задание A: Модуль "Hello, World!"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = "Hello from Brown Dennis module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

static int __init hello_init(void) {
    printk(KERN_INFO "=== Hello Module Loaded ===\n");
    printk(KERN_INFO "Message: %s\n", message);
    printk(KERN_INFO "Module loaded successfully!\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "=== Hello Module Unloaded ===\n");
    printk(KERN_INFO "Goodbye from kernel space!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brown Dennis");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");
Создать простой модуль ядра, который:

При загрузке (insmod) выводит в системный журнал приветственное сообщение

При выгрузке (rmmod) выводит прощальное сообщение

Принимает строковый параметр message

Команды:

bash
sudo insmod hello_module.ko
sudo rmmod hello_module
sudo insmod hello_module.ko message=Brown_Dennis_Test
sudo rmmod hello_module
Результат:

bash
[ 3665.384298] === Hello Module Loaded ===
[ 3665.384302] Message: Brown_Dennis_Test
[ 3665.384303] Module loaded successfully!
[ 3665.436986] === Hello Module Unloaded ===
[ 3665.436989] Goodbye from kernel space!
Вывод:
Модуль корректно загружается, выгружается и принимает параметры.

Задание B:
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#define PROC_NAME "my_config"
#define MAX_SIZE 256

static struct proc_dir_entry *proc_file;
static char *config_data;
static size_t config_size;

static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char buffer[MAX_SIZE];
    int len;
    
    if (*ppos > 0) return 0;
    
    len = snprintf(buffer, sizeof(buffer), "%s\n", config_data);
    
    if (copy_to_user(ubuf, buffer, len)) return -EFAULT;
    
    *ppos = len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char *temp_buf;
    
    if (count >= MAX_SIZE) return -EINVAL;
    
    temp_buf = kmalloc(count + 1, GFP_KERNEL);
    if (!temp_buf) return -ENOMEM;
    
    if (copy_from_user(temp_buf, ubuf, count)) {
        kfree(temp_buf);
        return -EFAULT;
    }
    
    temp_buf[count] = '\0';
    if (temp_buf[count - 1] == '\n') temp_buf[count - 1] = '\0';
    
    kfree(config_data);
    config_data = temp_buf;
    config_size = strlen(config_data);
    
    printk(KERN_INFO "Config updated to: %s\n", config_data);
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_init(void) {
    config_data = kstrdup("default", GFP_KERNEL);
    if (!config_data) return -ENOMEM;
    
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        kfree(config_data);
        return -ENOMEM;
    }
    
    printk(KERN_INFO "=== Proc Module Loaded ===\n");
    printk(KERN_INFO "/proc/%s created successfully\n", PROC_NAME);
    printk(KERN_INFO "Initial config: %s\n", config_data);
    return 0;
}

static void __exit proc_exit(void) {
    proc_remove(proc_file);
    kfree(config_data);
    printk(KERN_INFO "=== Proc Module Unloaded ===\n");
    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brown Dennis");
MODULE_DESCRIPTION("/proc file with read/write capability");
MODULE_VERSION("1.0");


 /proc файл с записью
Создать модуль, который создаёт файл /proc/my_config с возможностью записи:

По умолчанию содержит строку "default"

При записи сохраняет новое значение

При чтении возвращает текущее значение

Команды:

bash
sudo insmod proc_module.ko
ls -la /proc/my_config
cat /proc/my_config
echo "brown_dennis_value" | sudo tee /proc/my_config
cat /proc/my_config
sudo rmmod proc_module
Результат:

bash
# ls -la /proc/my_config
-rw-rw-rw- 1 root root 0 Nov 11 18:03 /proc/my_config

# Первое чтение:
default

# После записи:
brown_dennis_value

# Логи:
[ 3665.506462] === Proc Module Loaded ===
[ 3665.506466] /proc/my_config created successfully
[ 3665.506467] Initial config: default
[ 3665.557100] Config updated to: brown_dennis_value
[ 3665.635681] === Proc Module Unloaded ===
[ 3665.635687] /proc/my_config removed
Вывод:
Файл корректно создаётся в /proc, поддерживает операции чтения и записи, сохраняет данные между операциями. Файл удаляется при выгрузке модуля.

Задание C: /proc файл со статистикой системы

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/jiffies.h>

#define PROC_NAME "sys_stats"

static struct proc_dir_entry *proc_file;

static int count_processes(void) {
    struct task_struct *task;
    int count = 0;
    
    rcu_read_lock();
    for_each_process(task) count++;
    rcu_read_unlock();
    
    return count;
}

static void get_memory_info(unsigned long *total, unsigned long *used) {
    struct sysinfo mem_info;
    
    si_meminfo(&mem_info);
    
    *total = (mem_info.totalram * mem_info.mem_unit) / (1024 * 1024);
    *used = ((mem_info.totalram - mem_info.freeram) * mem_info.mem_unit) / (1024 * 1024);
}

static ssize_t stats_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char buffer[512];
    int len;
    unsigned long total_mem, used_mem;
    unsigned long uptime_seconds;
    int process_count;
    
    if (*ppos > 0) return 0;
    
    process_count = count_processes();
    get_memory_info(&total_mem, &used_mem);
    uptime_seconds = jiffies_to_msecs(jiffies) / 1000;
    
    len = snprintf(buffer, sizeof(buffer),
        "System Statistics:\n"
        "==================\n"
        "Processes: %d\n"
        "Memory Used: %lu MB / %lu MB\n"
        "System Uptime: %lu seconds\n"
        "Module loaded at jiffies: %lu\n",
        process_count, used_mem, total_mem, uptime_seconds, jiffies);
    
    if (copy_to_user(ubuf, buffer, len)) return -EFAULT;
    
    *ppos = len;
    return len;
}

static const struct proc_ops stats_fops = {
    .proc_read = stats_read,
};

static int __init stats_init(void) {
    proc_file = proc_create(PROC_NAME, 0444, NULL, &stats_fops);
    if (!proc_file) return -ENOMEM;
    
    printk(KERN_INFO "=== System Stats Module Loaded ===\n");
    printk(KERN_INFO "/proc/%s created successfully\n", PROC_NAME);
    return 0;
}

static void __exit stats_exit(void) {
    proc_remove(proc_file);
    printk(KERN_INFO "=== System Stats Module Unloaded ===\n");
    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

module_init(stats_init);
module_exit(stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brown Dennis");
MODULE_DESCRIPTION("System statistics via /proc interface");
MODULE_VERSION("1.0");

Создать модуль, который создаёт файл /proc/sys_stats с информацией:

Количество запущенных процессов

Используемая память

Uptime системы

Команды:

bash
sudo insmod sys_stats_module.ko
ls -la /proc/sys_stats
cat /proc/sys_stats
sudo rmmod sys_stats_module
Результат:

bash
# ls -la /proc/sys_stats
-r--r--r-- 1 root root 0 Nov 11 18:03 /proc/sys_stats

# Чтение статистики:
System Statistics:
==================
Processes: 246
Memory Used: 4373 MB / 7751 MB
System Uptime: 3365 seconds
Module loaded at jiffies: 4298332455

# Логи операций:
[ 3665.585792] === System Stats Module Loaded ===
[ 3665.585796] /proc/sys_stats created successfully
[ 3665.655368] === System Stats Module Unloaded ===
[ 3665.655371] /proc/sys_stats removed
Вывод:
Модуль корректно создаёт файл в /proc с актуальной статистикой системы, отображает информацию о процессах, памяти и времени работы. Файл удаляется при выгрузке модуля.

Ответы на вопросы
Базовые понятия
1. Что такое модуль ядра и зачем он нужен?
Модуль ядра - это фрагмент кода, который может быть динамически загружен и выгружен из ядра Linux во время работы системы без необходимости перезагрузки.
Модули нужны для:

Добавления драйверов устройств

Расширения функциональности ядра

Экономии памяти (загружаются только когда нужны)

Упрощения разработки и отладки

2. Чем отличается kernel-space от user-space?
Kernel-space: Привилегированный режим, полный доступ к оборудованию, сбой приводит к панике ядра (kernel panic). Использует API ядра.

User-space: Непривилегированный режим, доступ к ресурсам через системные вызовы, сбой затрагивает только процесс.

3. Что произойдёт, если в модуле обратиться к NULL указателю?
В модуле вызовет исключение в ядре, что приведет к kernel panic и аварийной остановке системы.

4. Почему нельзя использовать printf() в модуле ядра?
printf() - это функция стандартной библиотеки C, которая недоступна в kernel-space. Вместо этого используется printk() - аналог для kernel-space.

5. Что такое kernel panic и как его избежать?
Kernel panic - это неисправимая ошибка ядра, приводящая к остановке системы. Чтобы избежать: проверять указатели перед использованием, проверять возвращаемые значения функций, корректно освобождать ресурсы.

Жизненный цикл модуля
6. Какие функции вызываются при insmod и rmmod?
insmod: вызывается функция, указанная в module_init()

rmmod: вызывается функция, указанная в module_exit()

7. Что должна делать функция module_exit()?
Должна освобождать все ресурсы, выделенные модулем (память, /proc файлы, записи устройств).

8. Что происходит, если module_init() возвращает ошибку?
Модуль не загружается в систему, module_exit() не вызывается.

9. Можно ли выгрузить модуль, если он используется?
Нет, нельзя. Ядро отслеживает счетчик использования модуля. Модуль можно выгрузить только когда счетчик равен 0.

Логирование и отладка
10. Чем printk() отличается от printf()?
printk() работает в kernel-space, printf() в user-space

printk() использует уровни логирования (KERN_INFO, KERN_ERR, etc.)

printk() записывает в кольцевой буфер ядра, доступный через dmesg

11. Какие уровни логирования существуют в ядре?
KERN_EMERG - система неработоспособна

KERN_ALERT - необходимо немедленное вмешательство

KERN_CRIT - критические условия

KERN_ERR - ошибки

KERN_WARNING - предупреждения

KERN_NOTICE - нормальные, но значимые события

KERN_INFO - информационные сообщения

KERN_DEBUG - отладочные сообщения

12. Как посмотреть логи модуля?
bash
dmesg | tail                    # последние сообщения
dmesg | grep module_name        # фильтр по имени модуля
journalctl -k                   # через systemd
13. Что означает "tainted kernel"?
Флаг, указывающий, что ядро загрузило проприетарный модуль, модуль без лицензии или модуль, загруженный принудительно.

Память
14. Чем kmalloc() отличается от malloc()?
kmalloc() выделяет физически непрерывную память в kernel-space, malloc() в user-space

kmalloc() использует флаги GFP для управления выделением

15. Что такое флаги GFP и зачем они нужны?
GFP (Get Free Page) определяют контекст и приоритет выделения памяти:

GFP_KERNEL — стандартное выделение, может спать

GFP_ATOMIC — для атомарных контекстов, не спит

16. Что произойдёт, если не освободить память в module_exit()?
Утечка памяти ядра, память останется занятой до перезагрузки системы.

17. Почему нельзя использовать user-space указатели напрямую в ядре?
Указатели user-space нельзя использовать напрямую в ядре, так как они являются виртуальными адресами в адресном пространстве пользовательского процесса. Для работы с ними используются функции copy_from_user() и copy_to_user().

Взаимодействие с user-space
18. Что такое /proc и для чего он используется?
/proc - виртуальная файловая система для предоставления информации о системе и процессах, а также для простого взаимодействия с модулями ядра.

19. Что такое /sys (sysfs) и чем отличается от procfs?
/sys (sysfs) - файловая система для экспорта информации об устройствах и драйверах, более структурированная чем /proc.

20. Зачем нужны функции copy_to_user() и copy_from_user()?
Эти функции обеспечивают безопасное копирование данных между kernel-space и user-space, проверяют валидность адресов user-space.

21. Что такое character device и как он работает?
Character device — это устройство, с которым можно работать как с потоком байтов. Модуль регистрирует операции с устройством через структуру file_operations.

Параметры и метаданные
22. Как передать параметры модулю при загрузке?
Параметры модулю передаются при загрузке: insmod module.ko param_name=value.

23. Зачем нужен MODULE_LICENSE()?
Необходим для указания лицензии модуля. Модули без лицензии GPL помечают ядро как "tainted".

24. Что произойдёт, если не указать лицензию?
Если не указать лицензию, ядро пометит себя как "tainted", что может осложнить получение поддержки при отладке проблем.

Безопасность
25. Какие основные правила безопасного кода в ядре?
Проверять все возвращаемые значения

Использовать безопасные функции копирования

Правильно управлять памятью

Проверять границы буферов

26. Можно ли использовать бесконечный цикл в модуле?
Бесконечный цикл в модуле может полностью заблокировать ядро, особенно если он выполняется в атомарном контексте.

27. Почему в ядре нет FPU операций?
FPU операции в ядре не используются, потому что контекст ядра не сохраняет и не восстанавливает состояние FPU по умолчанию для производительности.

28. Что делать, если модуль вызвал kernel panic?
Перезагрузить систему

Проанализировать стек трейс в логах

Исправить ошибку в коде

Тестировать в виртуальной машине

Практические вопросы
29. Как узнать, какие модули загружены в системе?
bash
lsmod                    # список загруженных модулей
cat /proc/modules        # подробная информация
30. Как получить информацию о модуле (версия, параметры)?
bash
modinfo module.ko        # информация о файле модуля
ls /sys/module/module_name/parameters/  # параметры модуля
Использование AI
Корректное написание и форматирование файлов-отчётов, помощь в решении возникающих ошибок во время выполнения работы
