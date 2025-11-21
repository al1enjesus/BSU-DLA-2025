#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/compiler.h>

#define STATS_FILENAME "sys_stats"

static struct proc_dir_entry *proc_stats_file;

static int count_processes(void) {
    struct task_struct *task;
    int count = 0;
    rcu_read_lock();
    for_each_process(task) {
        count++;
    }
    rcu_read_unlock();
    return count;
}

#define PAGES_TO_MB(x) ((x) << (PAGE_SHIFT - 20)) 

static ssize_t stats_read(struct file *file, char __user *ubuf,
                          size_t count, loff_t *ppos) {
    char buf[512];
    int len;
    struct sysinfo i;
    unsigned long uptime_seconds;
    unsigned long total_mem_mb;
    unsigned long free_mem_mb;
    unsigned long used_mem_mb;
    int process_count;

    if (*ppos > 0)
        return 0;

    si_meminfo(&i);
    uptime_seconds = jiffies_to_msecs(jiffies) / 1000;
    process_count = count_processes();

    total_mem_mb = PAGES_TO_MB(i.totalram);
    free_mem_mb = PAGES_TO_MB(i.freeram);
    used_mem_mb = total_mem_mb - free_mem_mb;

    len = snprintf(buf, sizeof(buf),
                   "System Statistics:\n"
                   "  Processes: %d\n"
                   "  Memory Used: %lu MB (Total: %lu MB)\n"
                   "  System Uptime: %lu seconds\n",
                   process_count, used_mem_mb, total_mem_mb, uptime_seconds);

    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

static const struct proc_ops stats_proc_ops = {
    .proc_read = stats_read,
};

static int __init proc_stats_init(void) {
    proc_stats_file = proc_create(STATS_FILENAME, 0444, NULL, &stats_proc_ops);
    
    if (!proc_stats_file) {
        printk(KERN_ERR "Failed to create /proc/%s\n", STATS_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created successfully.\n", STATS_FILENAME);
    return 0;
}

static void __exit proc_stats_exit(void) {
    if (proc_stats_file) {
        proc_remove(proc_stats_file); 
        printk(KERN_INFO "/proc/%s removed.\n", STATS_FILENAME);
    }
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kuharevich Alexander");
MODULE_DESCRIPTION("Lab 5 (Even): /proc file with system statistics.");

module_init(proc_stats_init);
module_exit(proc_stats_exit);
