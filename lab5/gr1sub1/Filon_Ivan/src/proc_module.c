#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>

#define PROC_NAME "student_info"

static struct proc_dir_entry *proc_file;
static unsigned long load_time_jiffies;
static int read_count = 0;

static ssize_t student_info_read(struct file *file, char __user *buf,
                                 size_t count, loff_t *ppos)
{
    char info[256];
    int len;

    if (*ppos > 0)
        return 0;

    read_count++;

    len = snprintf(info, sizeof(info),
                   "Name: Filon Ivan\n"
                   "Group: 1, Subgroup: 1\n"
                   "Module loaded at: %lu jiffies\n"
                   "Read count: %d\n",
                   load_time_jiffies, read_count);

    if (copy_to_user(buf, info, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

static const struct proc_ops student_proc_ops = {
    .proc_read = student_info_read,
};

static int __init student_module_init(void)
{
    load_time_jiffies = jiffies;

    proc_file = proc_create(PROC_NAME, 0444, NULL, &student_proc_ops);
    if (!proc_file)
        return -ENOMEM;

    printk(KERN_INFO "[student_module] /proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit student_module_exit(void)
{
    proc_remove(proc_file);
    printk(KERN_INFO "[student_module] /proc/%s removed\n", PROC_NAME);
}

module_init(student_module_init);
module_exit(student_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Module creating /proc/student_info");
