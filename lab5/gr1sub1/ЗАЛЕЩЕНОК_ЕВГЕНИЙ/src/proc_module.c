#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h> 
#include <linux/uaccess.h> 
#include <linux/jiffies.h> 

#define PROC_FILENAME "student_info"

static unsigned long load_time_jiffies;
static atomic_t read_count; 

static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char buf[512];
    int len = 0;
    
    if (*ppos > 0) {
        return 0;
    }

    atomic_inc(&read_count);
    
    len = scnprintf(buf, sizeof(buf),
                    "Name: Zhenya\n"
                    "Group: 1, Subgroup: 1\n"
                    "Module loaded at: %lu jiffies\n"
                    "Read count: %d\n",
                    load_time_jiffies,
                    atomic_read(&read_count));

    if (copy_to_user(ubuf, buf, len)) {
        return -EFAULT; 
    }

    *ppos = len; 
    return len;
}

static const struct proc_ops my_proc_ops = {
    .proc_read = proc_read,
};

static int __init proc_init(void) {
    if (!proc_create(PROC_FILENAME, 0444, NULL, &my_proc_ops)) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }

    load_time_jiffies = jiffies; 
    atomic_set(&read_count, 0); 

    printk(KERN_INFO "/proc/%s created successfully.\n", PROC_FILENAME);
    return 0;
}

static void __exit proc_exit(void) {
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "/proc/%s removed.\n", PROC_FILENAME);
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhenya");
MODULE_DESCRIPTION("Module to create a /proc file with student info.");
