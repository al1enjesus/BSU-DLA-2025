#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Galkevich");
MODULE_DESCRIPTION("/proc/student_info");

#define PROC_NAME         "student_info"

#define STUDENT_NAME      "Victor Galkevich"
#define STUDENT_GROUP     1
#define STUDENT_SUBGROUP  1

static struct proc_dir_entry *proc_entry;
static unsigned long load_time_jiffies;
static atomic64_t read_count;

static ssize_t student_info_read(struct file *file, char __user *buf,
                                 size_t count, loff_t *ppos)
{
    char kbuf[256];
    int len;

    if (*ppos == 0)
        atomic64_inc(&read_count);

    len = scnprintf(kbuf, sizeof(kbuf),
                    "Name: %s\n"
                    "Group: %d, Subgroup: %d\n"
                    "Module loaded at: %lu jiffies\n"
                    "Read count: %lld\n",
                    STUDENT_NAME,
                    STUDENT_GROUP, STUDENT_SUBGROUP,
                    jiffies - load_time_jiffies,
                    (long long)atomic64_read(&read_count));

    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops student_info_ops = {
    .proc_read  = student_info_read,
    .proc_lseek = default_llseek,
};
#define PROC_FOPS (&student_info_ops)
#else
static const struct file_operations student_info_fops = {
    .owner  = THIS_MODULE,
    .read   = student_info_read,
    .llseek = default_llseek,
};
#define PROC_FOPS (&student_info_fops)
#endif

static int __init student_info_init(void)
{
    load_time_jiffies = jiffies;
    atomic64_set(&read_count, 0);

    proc_entry = proc_create(PROC_NAME, 0444, NULL, PROC_FOPS);
    if (!proc_entry)
        return -ENOMEM;

    pr_info("/proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit student_info_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);
    pr_info("/proc/%s removed\n", PROC_NAME);
}

module_init(student_info_init);
module_exit(student_info_exit);