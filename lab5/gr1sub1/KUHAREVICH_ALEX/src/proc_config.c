#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define CONFIG_FILENAME "my_config"
#define MAX_CONFIG_LEN  256

static struct proc_dir_entry *proc_file;
static char config_buffer[MAX_CONFIG_LEN];
static size_t config_len = 0;

static ssize_t config_read(struct file *file, char __user *ubuf,
                           size_t count, loff_t *ppos) {
    size_t len_to_copy;

    if (*ppos > 0 || config_len == 0)
        return 0;

    len_to_copy = min((size_t)count, config_len);

    if (copy_to_user(ubuf, config_buffer, len_to_copy))
        return -EFAULT;

    *ppos = len_to_copy;
    return len_to_copy;
}

static ssize_t config_write(struct file *file, const char __user *ubuf,
                            size_t count, loff_t *ppos) {
    size_t len_to_copy = min((size_t)count, (size_t)MAX_CONFIG_LEN);

    if (copy_from_user(config_buffer, ubuf, len_to_copy))
        return -EFAULT;

    config_len = len_to_copy;
    if (config_len < MAX_CONFIG_LEN)
        config_buffer[config_len] = '\0';
    
    printk(KERN_INFO "my_config: New value set. Length: %zu\n", config_len);

    return count; 
}

static const struct proc_ops config_proc_ops = {
    .proc_read = config_read,
    .proc_write = config_write,
};

static int __init proc_config_init(void) {
    snprintf(config_buffer, MAX_CONFIG_LEN, "default\n");
    config_len = strlen("default\n");

    proc_file = proc_create(CONFIG_FILENAME, 0666, NULL, &config_proc_ops);
    
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc/%s\n", CONFIG_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created successfully.\n", CONFIG_FILENAME);
    return 0;
}

static void __exit proc_config_exit(void) {
    if (proc_file) {
        proc_remove(proc_file); 
        printk(KERN_INFO "/proc/%s removed.\n", CONFIG_FILENAME);
    }
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kuharevich Alexander");
MODULE_DESCRIPTION("Lab 5 (Even): /proc file with write support for configuration.");

module_init(proc_config_init);
module_exit(proc_config_exit);
