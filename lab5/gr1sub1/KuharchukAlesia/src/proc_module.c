#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>

#define PROC_NAME "student_info"

static unsigned long load_time;
static unsigned int read_count = 0;

static int student_info_show(struct seq_file *m, void *v) {
    read_count++;
    
    seq_printf(m, "Name: Alesia\n");
    seq_printf(m, "Group: 1, Subgroup: 1\n");
    seq_printf(m, "Module loaded at: %lu jiffies\n", load_time);
    seq_printf(m, "Read count: %u\n", read_count);
    
    return 0;
}

static int student_info_open(struct inode *inode, struct file *file) {
    return single_open(file, student_info_show, NULL);
}

static const struct proc_ops student_info_fops = {
    .proc_open = student_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init proc_init(void) {
    load_time = jiffies;
    
    if (!proc_create(PROC_NAME, 0444, NULL, &student_info_fops)) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }
    
    printk(KERN_INFO "Created /proc/%s\n", PROC_NAME);
    return 0;
}

static void __exit proc_exit(void) {
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "Removed /proc/%s\n", PROC_NAME);
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alesia");
MODULE_DESCRIPTION("/proc file with student information for Variant 1");
