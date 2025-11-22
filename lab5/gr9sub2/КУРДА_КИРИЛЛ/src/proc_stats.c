#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>

#define PROC_NAME "sys_stats"

static struct proc_dir_entry *proc_file;

// Функция чтения (cat /proc/sys_stats)
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[512];
    int len;
    unsigned long uptime_sec;
    int process_count = 0;
    struct task_struct *task;
    struct sysinfo si;

    // Если уже читали - возвращаем 0
    if (*ppos > 0)
        return 0;

    // Подсчёт процессов
    for_each_process(task) {
        process_count++;
    }

    // Получение информации о памяти
    si_meminfo(&si);

    // Вычисление uptime (в секундах)
    uptime_sec = jiffies_to_msecs(jiffies) / 1000;

    // Формирование вывода
    len = snprintf(buf, sizeof(buf),
        "=== System Statistics ===\n"
        "Processes: %d\n"
        "Total RAM: %lu MB\n"
        "Free RAM: %lu MB\n"
        "Used RAM: %lu MB\n"
        "System Uptime: %lu seconds (%lu minutes)\n"
        "=========================\n",
        process_count,
        (si.totalram * si.mem_unit) / (1024 * 1024),
        (si.freeram * si.mem_unit) / (1024 * 1024),
        ((si.totalram - si.freeram) * si.mem_unit) / (1024 * 1024),
        uptime_sec,
        uptime_sec / 60
    );

    // Копируем в user-space
    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    *ppos = len;
    
    printk(KERN_INFO "sys_stats: statistics read (%d processes)\n", process_count);
    return len;
}

// Структура операций
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

// Инициализация модуля
static int __init proc_stats_init(void)
{
    // Создаём /proc файл (read-only)
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "sys_stats: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "sys_stats: module loaded, /proc/%s created\n", PROC_NAME);
    printk(KERN_INFO "sys_stats: try 'cat /proc/%s'\n", PROC_NAME);
    
    return 0;
}

// Выгрузка модуля
static void __exit proc_stats_exit(void)
{
    proc_remove(proc_file);
    printk(KERN_INFO "sys_stats: module unloaded, /proc/%s removed\n", PROC_NAME);
}

module_init(proc_stats_init);
module_exit(proc_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Kurda");
MODULE_DESCRIPTION("Lab5 Task C: /proc file with system statistics");
MODULE_VERSION("1.0");