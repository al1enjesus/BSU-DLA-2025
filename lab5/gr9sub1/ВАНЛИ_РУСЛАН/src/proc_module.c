#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>


#define PROC_FILENAME "kernel_task_info"
#define TEMP_BUF_SIZE 256

static struct proc_dir_entry *entry_ptr = NULL;
static int access_counter = 0;
static unsigned long start_jiffies = 0;

static ssize_t info_read_handler(struct file *file, char __user *user_buffer,
                                 size_t count, loff_t *position)
{
    char k_buf[TEMP_BUF_SIZE];
    int str_len;

 
    if (*position != 0)
        return 0;

    access_counter++;


    str_len = scnprintf(k_buf, sizeof(k_buf),
        "=== Student Kernel Module ===\n"
        "Author: [Vanli Ruslan]\n"
        "Stats -> Reads: %d | Start Time: %lu\n",
        access_counter, start_jiffies);

 
    if (copy_to_user(user_buffer, k_buf, str_len))
        return -EFAULT;

    *position = str_len;

    return str_len;
}


static const struct proc_ops my_proc_ops = {
    .proc_read = info_read_handler,
};

static int __init proc_test_init(void)
{
    printk(KERN_INFO "kernel_task: Initializing proc entry\n");

    start_jiffies = jiffies;

    
    entry_ptr = proc_create(PROC_FILENAME, 0444, NULL, &my_proc_ops);
    if (!entry_ptr) {
        printk(KERN_ERR "kernel_task: Error creating /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "kernel_task: Interface /proc/%s created\n", PROC_FILENAME);

    return 0;
}

static void __exit proc_test_exit(void)
{
    if (entry_ptr) {
        proc_remove(entry_ptr);
        printk(KERN_INFO "kernel_task: /proc/%s removed\n", PROC_FILENAME);
    }

    printk(KERN_INFO "kernel_task: Module unloaded. Total accesses: %d\n", access_counter);
}

module_init(proc_test_init);
module_exit(proc_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("[Vanli Ruslan]");
MODULE_DESCRIPTION("Modified proc filesystem module");
MODULE_VERSION("2.0");
