#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kazlouski Anton");
MODULE_DESCRIPTION("/proc file with student information");
MODULE_VERSION("1.0");

#define PROC_NAME "student_info"

static unsigned long load_time;
static unsigned int read_count = 0;

static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
char buf[256];
int len;

if (*ppos > 0) {
return 0;
}

read_count++;

len = snprintf(buf, sizeof(buf),
               "Name: Kazlouski Anton\n"
               "Group: 1, Subgroup: 1\n"
               "Module loaded at: %lu jiffies\n"
               "Current time: %lu jiffies\n"
               "Read count: %u\n",
               load_time, jiffies, read_count);

if (copy_to_user(ubuf, buf, len)) {
return -EFAULT;
}

*ppos = len;
return len;
}

static const struct proc_ops proc_ops = {
    .proc_read = proc_read,
};

static int __init proc_init(void) {
    load_time = jiffies;

    if (!proc_create(PROC_NAME, 0444, NULL, &proc_ops)) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created successfully\n", PROC_NAME);
    printk(KERN_INFO "Proc module loaded at %lu jiffies\n", load_time);
    return 0;
}

static void __exit proc_exit(void) {
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
    printk(KERN_INFO "Proc module unloaded after %lu jiffies\n", jiffies - load_time);
}

module_init(proc_init);
module_exit(proc_exit);