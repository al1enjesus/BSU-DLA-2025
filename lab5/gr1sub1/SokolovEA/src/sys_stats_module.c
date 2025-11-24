#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>

#define PROC_NAME "sys_stats"
#define MAX_SIZE 512

// Глобальные переменные
static struct proc_dir_entry *proc_file = NULL;

// Функция для подсчёта процессов
static int count_processes(void)
{
    struct task_struct *task;
    int count = 0;

    rcu_read_lock();
    for_each_process(task) {
        count++;
    }
    rcu_read_unlock();

    return count;
}

// Функция для получения информации о памяти (в MB)
static long get_memory_usage_mb(void)
{
    struct sysinfo si;
    
    si_meminfo(&si);
    
    // Возвращаем использованную память в MB
    // (Общая память - свободная память) * размер страницы / (1024*1024)
    return ((si.totalram - si.freeram) * si.mem_unit) >> 20;
}

// Функция для получения uptime в секундах
static unsigned long get_uptime_seconds(void)
{
    return jiffies_to_msecs(jiffies) / 1000;
}

// Функция чтения из /proc файла - вызывается при "cat /proc/sys_stats"
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char *buf;
    int len;
    int processes;
    long memory_mb;
    unsigned long uptime;

    // Если уже читали (повторный вызов), возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Выделяем буфер для формирования ответа
    buf = kmalloc(MAX_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    // Собираем статистику
    processes = count_processes();
    memory_mb = get_memory_usage_mb();
    uptime = get_uptime_seconds();

    // Форматируем вывод
    len = snprintf(buf, MAX_SIZE,
        "System Statistics\n"
        "=================\n"
        "Processes: %d\n"
        "Memory Used: %ld MB\n"
        "System Uptime: %lu seconds\n"
        "Collection time: %lu jiffies\n",
        processes, memory_mb, uptime, jiffies);

    // Проверяем, что данные поместились в буфер пользователя
    if (len > count) {
        kfree(buf);
        return -EINVAL;
    }

    // Копируем данные в user-space
    if (copy_to_user(ubuf, buf, len)) {
        kfree(buf);
        return -EFAULT;
    }

    kfree(buf);
    *ppos = len;
    
    printk(KERN_INFO "sys_stats: Statistics accessed\n");
    return len;
}

// Структура операций для proc файла
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

// Функция инициализации модуля
static int __init sys_stats_init(void)
{
    // Создаём proc файл только для чтения
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "sys_stats: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "sys_stats: Module loaded, /proc/%s created\n", PROC_NAME);
    return 0;
}

// Функция выгрузки модуля
static void __exit sys_stats_exit(void)
{
    // Удаляем proc файл
    if (proc_file) {
        proc_remove(proc_file);
        printk(KERN_INFO "sys_stats: /proc/%s removed\n", PROC_NAME);
    }

    printk(KERN_INFO "sys_stats: Module unloaded\n");
}

module_init(sys_stats_init);
module_exit(sys_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sokolov Evgeny");
MODULE_DESCRIPTION("System statistics proc module");
MODULE_VERSION("1.0");