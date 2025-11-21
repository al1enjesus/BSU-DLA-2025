
/*
 * proc_stats_module.c - Задание C: /proc файл со статистикой системы
 *
 * Создаёт файл /proc/sys_stats с информацией:
 * - Количество запущенных процессов (приблизительно)
 * - Используемая память
 * - Uptime системы
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>      // jiffies, jiffies_to_msecs
#include <linux/mm.h>           // si_meminfo
#include <linux/sched/signal.h> // for_each_process

#define PROC_NAME "sys_stats"
#define BUFFER_SIZE 512

static struct proc_dir_entry *proc_file;

// Функция подсчёта процессов
static int count_processes(void)
{
    struct task_struct *task;
    int count = 0;

    // Перебираем все процессы в системе
    for_each_process(task)
    {
        count++;
    }

    return count;
}

// Функция получения информации о памяти
static void get_memory_info(unsigned long *total_mb, unsigned long *used_mb,
                            unsigned long *free_mb)
{
    struct sysinfo si;

    si_meminfo(&si);

    // Конвертируем страницы в мегабайты
    // si.totalram * si.mem_unit даёт байты, делим на 1024*1024 для MB
    *total_mb = (si.totalram * si.mem_unit) >> 20; // >> 20 эквивалентно / (1024*1024)
    *free_mb = (si.freeram * si.mem_unit) >> 20;
    *used_mb = *total_mb - *free_mb;
}

// Функция чтения - вызывается при cat /proc/sys_stats
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buffer[BUFFER_SIZE];
    int len;
    int process_count;
    unsigned long total_mem, used_mem, free_mem;
    unsigned long uptime_seconds;

    // Если уже читали - возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Собираем статистику
    process_count = count_processes();
    get_memory_info(&total_mem, &used_mem, &free_mem);
    uptime_seconds = jiffies_to_msecs(jiffies) / 1000; // jiffies -> msecs -> seconds

    // Форматируем вывод
    len = snprintf(buffer, sizeof(buffer),
                   "=== System Statistics ===\n"
                   "Processes:     %d\n"
                   "Memory Total:  %lu MB\n"
                   "Memory Used:   %lu MB\n"
                   "Memory Free:   %lu MB\n"
                   "System Uptime: %lu seconds (%lu min, %lu hours)\n"
                   "========================\n",
                   process_count,
                   total_mem,
                   used_mem,
                   free_mem,
                   uptime_seconds,
                   uptime_seconds / 60,
                   uptime_seconds / 3600);

    // Проверяем границы
    if (count < len)
        len = count;

    // Копируем в user-space
    if (copy_to_user(ubuf, buffer, len))
    {
        printk(KERN_ERR "sys_stats: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos = len;

    printk(KERN_INFO "sys_stats: Statistics read (processes=%d, used_mem=%lu MB, uptime=%lu sec)\n",
           process_count, used_mem, uptime_seconds);

    return len;
}

// Структура операций для proc файла
static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
};

// Инициализация модуля
static int __init proc_stats_init(void)
{
    // Создаём файл /proc/sys_stats с правами 0444 (r--r--r--)
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);

    if (!proc_file)
    {
        printk(KERN_ERR "sys_stats: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "sys_stats: Module loaded, created /proc/%s\n", PROC_NAME);

    return 0;
}

// Выгрузка модуля
static void __exit proc_stats_exit(void)
{
    // Удаляем proc файл
    proc_remove(proc_file);

    printk(KERN_INFO "sys_stats: Module unloaded, removed /proc/%s\n", PROC_NAME);
}

module_init(proc_stats_init);
module_exit(proc_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Sadovskiy anton.crykov@gmail.com");
MODULE_DESCRIPTION("/proc file with system statistics (processes, memory, uptime)");
MODULE_VERSION("1.0");
