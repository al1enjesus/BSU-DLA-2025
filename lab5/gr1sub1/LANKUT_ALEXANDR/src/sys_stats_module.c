#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>   
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/mm.h>

static struct proc_dir_entry *proc_file;

/*
 * Функция подсчёта активных процессов
 */
static int count_processes(void)
{
    struct task_struct *task;
    int count = 0;

    // Проходим по всем процессам в системе
    for_each_process(task) {
        count++;
    }

    return count;
}

/*
 * Функция получения информации о памяти
 */
static void get_memory_info(unsigned long *used, unsigned long *total)
{
    struct sysinfo info;

    si_meminfo(&info);

    // Общая память в MB
    *total = (info.totalram * info.mem_unit) / (1024 * 1024);
    
    // Используемая память в MB
    *used = ((info.totalram - info.freeram) * info.mem_unit) / (1024 * 1024);
}

/*
 * Функция чтения /proc/sys_stats
 */
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[512];
    int len;
    int processes;
    unsigned long mem_used, mem_total;
    unsigned long uptime_ms;

    printk(KERN_INFO "sys_stats_module: read called\n");

    // Если уже читали, возвращаем 0
    if (*ppos > 0)
        return 0;

    // Получаем данные
    processes = count_processes();
    get_memory_info(&mem_used, &mem_total);
    uptime_ms = jiffies_to_msecs(jiffies);

    // Форматируем вывод
    len = snprintf(buf, sizeof(buf),
                   "System Statistics:\n"
                   "==================\n"
                   "Processes: %d\n"
                   "Memory Used: %lu MB\n"
                   "Memory Total: %lu MB\n"
                   "System Uptime: %lu seconds\n",
                   processes,
                   mem_used,
                   mem_total,
                   uptime_ms / 1000);

    // Копируем данные в user-space
    if (copy_to_user(ubuf, buf, len)) {
        printk(KERN_ERR "sys_stats_module: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos = len;
    return len;
}

// Структура file_operations для /proc файла
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

/*
 * Функция инициализации модуля
 */
static int __init sys_stats_init(void)
{
    printk(KERN_INFO "sys_stats_module: Initializing...\n");

    // Создаём /proc файл
    // Параметры: имя, права доступа, родительская папка, file_ops
    proc_file = proc_create("sys_stats", 0444, NULL, &proc_fops);

    if (!proc_file) {
        printk(KERN_ERR "sys_stats_module: Failed to create /proc/sys_stats\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "sys_stats_module: /proc/sys_stats created successfully\n");
    return 0;
}

/*
 * Функция выгрузки модуля
 */
static void __exit sys_stats_exit(void)
{
    // Удаляем /proc файл
    proc_remove(proc_file);

    printk(KERN_INFO "sys_stats_module: /proc/sys_stats removed\n");
}

module_init(sys_stats_init);
module_exit(sys_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ланкуть Александр");
MODULE_DESCRIPTION("Kernel module with system statistics in /proc");
MODULE_VERSION("1.0");
