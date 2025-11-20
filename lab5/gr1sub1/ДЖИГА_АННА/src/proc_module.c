#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define CONFIG_FILE      "my_config"
#define BUF_MAX          256

static struct proc_dir_entry *entry;
static char *buffer;
static DEFINE_MUTEX(config_lock);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanna");
MODULE_DESCRIPTION("Task B: Read/write /proc entry with thread safety.");
MODULE_VERSION("1.1");

static ssize_t config_read(struct file *file, char __user *user_buf, size_t len, loff_t *offset)
{
    ssize_t bytes_read;
    int data_len;

    if (*offset > 0)
        return 0;

    mutex_lock(&config_lock);

    data_len = strlen(buffer);
    if (copy_to_user(user_buf, buffer, data_len)) {
        mutex_unlock(&config_lock);
        pr_err("Failed to copy data to user space.\n");
        return -EFAULT;
    }

    *offset = data_len;
    bytes_read = data_len;

    mutex_unlock(&config_lock);
    return bytes_read;
}

static ssize_t config_write(struct file *file, const char __user *user_buf, size_t len, loff_t *offset)
{
    size_t write_len = len;

    if (write_len >= BUF_MAX) {
        pr_warn("Input too large; truncating to %d bytes.\n", BUF_MAX - 1);
        write_len = BUF_MAX - 1;
    }

    mutex_lock(&config_lock);

    if (copy_from_user(buffer, user_buf, write_len)) {
        mutex_unlock(&config_lock);
        pr_err("Failed to copy data from user space.\n");
        return -EFAULT;
    }

    buffer[write_len] = '\0';

    if (write_len > 0 && buffer[write_len - 1] == '\n')
        buffer[write_len - 1] = '\0';

    *offset = write_len;
    mutex_unlock(&config_lock);

    return write_len;
}

static const struct proc_ops config_fops = {
    .proc_read  = config_read,
    .proc_write = config_write,
};

static int __init config_init(void)
{
    buffer = kmalloc(BUF_MAX, GFP_KERNEL);
    if (!buffer) {
        pr_err("Memory allocation failed for buffer.\n");
        return -ENOMEM;
    }

    strscpy(buffer, "default", BUF_MAX);

    entry = proc_create(CONFIG_FILE, 0666, NULL, &config_fops);
    if (!entry) {
        kfree(buffer);
        pr_err("Could not create /proc/%s.\n", CONFIG_FILE);
        return -ENOMEM;
    }

    pr_info("/proc/%s initialized.\n", CONFIG_FILE);
    return 0;
}

static void __exit config_exit(void)
{
    proc_remove(entry);
    kfree(buffer);
    pr_info("/proc/%s cleaned up.\n", CONFIG_FILE);
}

module_init(config_init);
module_exit(config_exit);