#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/jiffies.h>

#define PROC_NAME "sys_stats"
#define BUFFER_SIZE 1024

static struct proc_dir_entry *proc_entry;

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

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char buffer[BUFFER_SIZE];
    int len = 0;
    struct sysinfo si;
    unsigned long uptime_seconds;
    int process_count;

    if (*ppos > 0)
        return 0;

    si_meminfo(&si);
    uptime_seconds = jiffies_to_msecs(get_jiffies_64()) / 1000;
    process_count = count_processes();

    len = snprintf(buffer, BUFFER_SIZE,
                  "Processes: %d\n"
                  "Memory Total: %lu MB\n"
                  "Memory Free: %lu MB\n"
                  "Memory Used: %lu MB\n"
                  "System Uptime: %lu seconds\n",
                  process_count,
                  (si.totalram * si.mem_unit) / (1024 * 1024),
                  (si.freeram * si.mem_unit) / (1024 * 1024),
                  ((si.totalram - si.freeram) * si.mem_unit) / (1024 * 1024),
                  uptime_seconds);

    if (copy_to_user(buf, buffer, len)) {
        return -EFAULT;
    }

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
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created successfully\n", PROC_NAME);
    return 0;
}

static void __exit sys_stats_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

module_init(sys_stats_init);
module_exit(sys_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ваше Имя");
MODULE_DESCRIPTION("System statistics module");