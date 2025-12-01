#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>
#include <linux/sysinfo.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanna");
MODULE_DESCRIPTION("Task C: Shows system stats via /proc/sys_stats.");
MODULE_VERSION("1.1");

static struct proc_dir_entry *stats_entry;

static int display_stats(struct seq_file *seq, void *unused)
{
    struct task_struct *task;
    int running = 0;
    struct sysinfo mem_info;
    unsigned long long uptime;

    if (unlikely(!seq))
        return -EINVAL;

    rcu_read_lock();
    for_each_process(task) {
        if (task->__state == TASK_RUNNING)
            running++;
    }
    rcu_read_unlock();

    si_meminfo(&mem_info);
    uptime = jiffies_to_msecs(jiffies) / 1000ULL;

    seq_printf(seq, "Processes: %d\n", running);
    seq_printf(seq, "Memory Used: %llu MB\n",
               ((u64)(mem_info.totalram - mem_info.freeram) * mem_info.mem_unit) >> 20);
    seq_printf(seq, "System Uptime: %llu seconds\n", uptime);

    return 0;
}

static int __init stats_init(void)
{
    stats_entry = proc_create_single("sys_stats", 0444, NULL, display_stats);
    if (!stats_entry) {
        pr_err("Failed to register /proc/sys_stats.\n");
        return -ENOMEM;
    }

    pr_info("System stats module loaded: /proc/sys_stats available.\n");
    return 0;
}

static void __exit stats_exit(void)
{
    if (stats_entry) {
        remove_proc_entry("sys_stats", NULL);
        pr_info("/proc/sys_stats removed.\n");
    } else {
        pr_warn("Attempted to remove non-existent /proc/sys_stats.\n");
    }
}

module_init(stats_init);
module_exit(stats_exit);