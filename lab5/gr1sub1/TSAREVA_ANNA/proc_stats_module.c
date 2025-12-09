#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>

#define STAT_PROC_NAME    "sys_stats"
#define OUTPUT_BUF_SIZE   512

static struct proc_dir_entry *stats_proc_entry = NULL;
static ssize_t stats_read_callback(struct file *filp,
                                   char __user *user_buffer,
                                   size_t buffer_size,
                                   loff_t *file_offset)
{
    char local_buffer[OUTPUT_BUF_SIZE];
    unsigned long process_total = 0UL;
    unsigned long memory_used_mb = 0UL;
    unsigned long uptime_in_sec = 0UL;
    int formatted_len = 0;

    if (*file_offset > 0)
        return 0;
    rcu_read_lock();
    {
        struct task_struct *iter_task;
        for_each_process(iter_task) {
            ++process_total;
        }
    }
    rcu_read_unlock();
    {
        struct sysinfo mem_info;
        si_meminfo(&mem_info);

        unsigned long total_ram_pages = mem_info.totalram;
        unsigned long free_ram_pages  = mem_info.freeram;
        unsigned long used_ram_pages  = (total_ram_pages > free_ram_pages)
                                      ? (total_ram_pages - free_ram_pages)
                                      : 0UL;
        memory_used_mb = (used_ram_pages * PAGE_SIZE) >> 20;  // divide by 2^20
    }

    uptime_in_sec = jiffies_to_msecs(jiffies) / 1000ULL;

    formatted_len = scnprintf(local_buffer,
                              sizeof(local_buffer),
                              "Processes: %lu\n"
                              "Memory Used: %lu MB\n"
                              "System Uptime: %lu seconds\n",
                              process_total,
                              memory_used_mb,
                              uptime_in_sec);

    if ((size_t)formatted_len > buffer_size)
        return -EINVAL;

    if (copy_to_user(user_buffer, local_buffer, formatted_len))
        return -EFAULT;

    return formatted_len;
}

static const struct proc_ops stats_file_operations = {
    .proc_read = stats_read_callback,
};

static int __init stats_module_init(void)
{
    stats_proc_entry = proc_create(STAT_PROC_NAME, 0444, NULL, &stats_file_operations);
    if (!stats_proc_entry) {
        pr_err("proc_stats_module: Failed to register /proc/%s\n", STAT_PROC_NAME);
        return -ENOMEM;
    }

    pr_info("proc_stats_module: /proc/%s exposed (read-only)\n", STAT_PROC_NAME);
    return 0;
}

static void __exit stats_module_exit(void)
{
    if (stats_proc_entry) {
        proc_remove(stats_proc_entry);
        stats_proc_entry = NULL;
        pr_info("proc_stats_module: /proc/%s unregistered\n", STAT_PROC_NAME);
    }
}

module_init(stats_module_init);
module_exit(stats_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsareva Anna");
MODULE_DESCRIPTION("/proc/sys_stats — lightweight system monitoring (Lab 5 Task C)");
MODULE_VERSION("1.0");