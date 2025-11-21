#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>
#include <linux/errno.h>
#include <linux/sysinfo.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gorbach Vladimir");
MODULE_DESCRIPTION("Task C: A simple module to display system stats in /proc/sys_stats with error handling.");
MODULE_VERSION("1.1");

static struct proc_dir_entry *proc_file;

/**
 * sys_stats_show - Функция формирования содержимого /proc/sys_stats
 * Собирает статистику о процессах, памяти и uptime системы
 */
static int sys_stats_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    int running_procs = 0;
    struct sysinfo si;
    unsigned long long uptime_secs;

    if (!m) {
        pr_err("sys_stats: invalid seq_file pointer.\n");
        return -EINVAL;
    }

    /* RCU lock для безопасной итерации по списку процессов */
    rcu_read_lock();
    for_each_process(task) {
        if (task->__state == TASK_RUNNING)
            running_procs++;
    }
    rcu_read_unlock();

    /* Получаем информацию о памяти системы */
    si_meminfo(&si);

    /* Конвертируем jiffies в секунды */
    uptime_secs = jiffies_to_msecs(jiffies) / 1000;

    seq_printf(m, "Processes: %d\n", running_procs);
    seq_printf(m, "Memory Used: %llu MB\n",
                ((u64)(si.totalram - si.freeram) * si.mem_unit) / (1024 * 1024));
    seq_printf(m, "System Uptime: %llu seconds\n", uptime_secs);

    return 0;
}

/**
 * sys_stats_init - Функция инициализации модуля
 * Создаёт файл /proc/sys_stats с правами только для чтения
 */
static int __init sys_stats_init(void)
{
    proc_file = proc_create_single("sys_stats", 0444, NULL, sys_stats_show);
    if (!proc_file) {
        pr_err("sys_stats: failed to create /proc/sys_stats (out of memory or already exists).\n");
        return -ENOMEM;
    }

    pr_info("sys_stats: /proc/sys_stats created successfully.\n");
    return 0;
}

/**
 * sys_stats_exit - Функция выгрузки модуля
 * Удаляет файл /proc/sys_stats
 */
static void __exit sys_stats_exit(void)
{
    if (proc_file) {
        remove_proc_entry("sys_stats", NULL);
        pr_info("sys_stats: /proc/sys_stats removed.\n");
    } else {
        pr_warn("sys_stats: no /proc/sys_stats entry to remove.\n");
    }
}

module_init(sys_stats_init);
module_exit(sys_stats_exit);
