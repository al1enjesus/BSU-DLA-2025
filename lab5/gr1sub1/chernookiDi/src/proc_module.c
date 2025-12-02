#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("System statistics in /proc");

#define PROC_NAME "sys_stats"
#define BUFFER_SIZE 1024

static struct proc_dir_entry *proc_entry;

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *buffer;
    ssize_t len = 0;
    struct task_struct *task;
    int process_count = 0;
    struct sysinfo mem_info;
    unsigned long uptime_seconds;

    if (*ppos > 0)
        return 0;

    buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

    rcu_read_lock();
    for_each_process(task) {
        process_count++;
    }
    rcu_read_unlock();

    si_meminfo(&mem_info);
    unsigned long used_mem_mb = (mem_info.totalram - mem_info.freeram) *
                               mem_info.mem_unit / (1024 * 1024);

    uptime_seconds = jiffies_to_msecs(jiffies) / 1000;

    len = snprintf(buffer, BUFFER_SIZE,
                  "Processes: %d\n"
                  "Memory Used: %lu MB\n"
                  "System Uptime: %lu seconds\n",
                  process_count, used_mem_mb, uptime_seconds);

    if (copy_to_user(buf, buffer, len)) {
        kfree(buffer);
        return -EFAULT;
    }

    kfree(buffer);
    *ppos = len;
    return len;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

static int __init sys_stats_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_entry) {
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit sys_stats_exit(void)
{
    proc_remove(proc_entry);
    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

module_init(sys_stats_init);
module_exit(sys_stats_exit);