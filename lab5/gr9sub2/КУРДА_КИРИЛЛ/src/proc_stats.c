#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/sysinfo.h>
#include <linux/rcupdate.h>
#include <linux/errno.h>

#define PROC_NAME "sys_stats"
#define STATS_BUF_SIZE 512 /* объяснение: размер буфера для текстового отчёта */

static struct proc_dir_entry *proc_file;

/* чтение: формируем строку в стековый буфер, учитываем частичное чтение */
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[STATS_BUF_SIZE];
    int len;
    unsigned long uptime_sec;
    int process_count = 0;
    struct task_struct *task;
    struct sysinfo si;

    if (!ubuf)
        return -EFAULT;

    /* Формируем статистику */
    rcu_read_lock();
    for_each_process(task) {
        process_count++;
    }
    rcu_read_unlock();

    si_meminfo(&si);

    uptime_sec = jiffies_to_msecs(jiffies) / 1000;

    len = snprintf(buf, sizeof(buf),
        "=== System Statistics ===\n"
        "Processes: %d\n"
        "Total RAM: %llu MB\n"
        "Free RAM: %llu MB\n"
        "Used RAM: %llu MB\n"
        "System Uptime: %lu seconds (%lu minutes)\n"
        "=========================\n",
        process_count,
        (unsigned long long)((si.totalram * si.mem_unit) / (1024ULL * 1024ULL)),
        (unsigned long long)((si.freeram  * si.mem_unit) / (1024ULL * 1024ULL)),
        (unsigned long long)(((si.totalram - si.freeram) * si.mem_unit) / (1024ULL * 1024ULL)),
        uptime_sec,
        uptime_sec / 60
    );

    if (len < 0)
        return -EFAULT;

    /* Если строка была обрезана (len >= sizeof(buf)), сообщаем и обрезаем len */
    if ((size_t)len >= sizeof(buf))
        len = sizeof(buf) - 1;

    /* Поддержка частичного чтения */
    if ((size_t)*ppos >= (size_t)len)
        return 0;

    if (count > (size_t)(len - *ppos))
        count = len - *ppos;

    if (copy_to_user(ubuf, buf + *ppos, count))
        return -EFAULT;

    *ppos += count;

    printk(KERN_INFO "sys_stats: statistics read (%d processes)\n", process_count);
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

static int __init proc_stats_init(void)
{
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "sys_stats: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "sys_stats: module loaded, /proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit proc_stats_exit(void)
{
    if (proc_file) {
        proc_remove(proc_file);
        proc_file = NULL;
    }

    printk(KERN_INFO "sys_stats: module unloaded, /proc/%s removed\n", PROC_NAME);
}

module_init(proc_stats_init);
module_exit(proc_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Kurda");
MODULE_DESCRIPTION("Lab5 Task C: /proc file with system statistics (hardened)");
MODULE_VERSION("1.1");
