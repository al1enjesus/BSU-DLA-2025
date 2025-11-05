#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>

#define PROC_NAME "my_config"
#define MAX_SIZE 256

static struct proc_dir_entry *proc_entry;
static char *config_data;
static size_t config_size;

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    if (*ppos > 0)
        return 0;

    if (copy_to_user(buf, config_data, config_size)) {
        return -EFAULT;
    }

    *ppos = config_size;
    return config_size;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    size_t actual_count = count;

    // Учитываем символ новой строки
    if (count > 0 && buf[count-1] == '\n') {
        actual_count = count - 1;
    }

    if (actual_count > MAX_SIZE - 1) {
        printk(KERN_WARNING "Config data too large\n");
        return -EINVAL;
    }

    if (copy_from_user(config_data, buf, actual_count)) {
        return -EFAULT;
    }

    config_data[actual_count] = '\0';
    config_size = actual_count;

    printk(KERN_INFO "Config updated to: %s\n", config_data);
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_config_init(void)
{
    config_data = kmalloc(MAX_SIZE, GFP_KERNEL);
    if (!config_data) {
        printk(KERN_ERR "Failed to allocate memory\n");
        return -ENOMEM;
    }

    strcpy(config_data, "default");
    config_size = strlen(config_data);

    // Исправлены права доступа с 0666 на 0644
    proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_fops);
    if (!proc_entry) {
        kfree(config_data);
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created successfully\n", PROC_NAME);
    return 0;
}

static void __exit proc_config_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    if (config_data)
        kfree(config_data);

    printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

module_init(proc_config_init);
module_exit(proc_config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Polina");
MODULE_DESCRIPTION("/proc config file module");