/*
 * proc_stats_module.c - /proc/sys_stats read-only module (Lab 5, Task C)
 *
 * Exports system statistics on read:
 *  - number of processes
 *  - approximate memory used (MB)
 *  - system uptime (seconds)
 *
 * Build / Test:
 *   make
 *   sudo insmod proc_stats_module.ko
 *   cat /proc/sys_stats
 *   sudo rmmod proc_stats_module
 *   dmesg | tail -5
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>

#define PROC_STATS_NAME  "sys_stats"
#define STATS_BUF_SIZE   512

static struct proc_dir_entry *proc_stats_entry  = NULL;

/* Read handler for /proc/sys_stats */
static ssize_t proc_stats_read(struct file *file, char __user *ubuf,
                               size_t count, loff_t *ppos)
{
    char buf[STATS_BUF_SIZE];
    int len = 0;
    unsigned long proc_count = 0;
    struct task_struct *task;
    unsigned long used_mb = 0;
    unsigned long uptime_sec = 0;

    if (*ppos > 0)
        return 0;

    rcu_read_lock();
    for_each_process(task) {
        proc_count++;
    }
    rcu_read_unlock();

    {
        struct sysinfo i;
        si_meminfo(&i);
        unsigned long total_pages = i.totalram;
        unsigned long free_pages  = i.freeram;
        unsigned long used_pages  = (total_pages > free_pages) ? (total_pages - free_pages) : 0;
        used_mb = (used_pages * PAGE_SIZE) >> 20;
    }

    uptime_sec = (jiffies_to_msecs(jiffies)) / 1000ULL;

    len = scnprintf(buf, sizeof(buf),
                    "Processes: %lu\n"
                    "Memory Used: %lu MB\n"
                    "System Uptime: %lu seconds\n",
                    proc_count, used_mb, uptime_sec);

    if (len > count)
        return -EINVAL;

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

static const struct proc_ops proc_stats_ops = {
    .proc_read = proc_stats_read,
};

static int __init proc_stats_module_init(void)
{
    proc_stats_entry = proc_create(PROC_STATS_NAME, 0444, NULL, &proc_stats_ops);
    if (!proc_stats_entry) {
        pr_err("proc_stats_module: Failed to create /proc/%s\n", PROC_STATS_NAME);
        return -ENOMEM;
    }

    pr_info("proc_stats_module: Created /proc/%s (read-only)\n", PROC_STATS_NAME);
    return 0;
}

static void __exit proc_stats_module_exit(void)
{
    if (proc_stats_entry) {
        proc_remove(proc_stats_entry);
        proc_stats_entry = NULL;
        pr_info("proc_stats_module: Removed /proc/%s\n", PROC_STATS_NAME);
    }
}

module_init(proc_stats_module_init);
module_exit(proc_stats_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladislav Sokov");
MODULE_DESCRIPTION("/proc/sys_stats - simple system statistics (Lab 5 Task C)");
MODULE_VERSION("1.0");
