/*
 * proc_config_module.c - /proc/my_config read/write module (Lab 5, Task B)
 *
 * Behavior:
 *  - Creates /proc/my_config with permissions 0666
 *  - Default content: "default"
 *  - Supports write: echo "text" > /proc/my_config
 *  - Supports read:  cat /proc/my_config
 *  - Max length (including terminating NUL): 256 bytes
 *
 * Build / Test:
 *   make
 *   sudo insmod proc_config_module.ko
 *   cat /proc/my_config        # should print "default"
 *   echo "new value" > /proc/my_config
 *   cat /proc/my_config        # should print "new value"
 *   sudo rmmod proc_config_module
 *   dmesg | tail -5
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define PROC_CONFIG_NAME "my_config"
#define CONFIG_MAX_LEN   256

static struct proc_dir_entry *proc_config_entry = NULL;
static char config_buf[CONFIG_MAX_LEN];
static size_t config_len = 0;

/* Read handler for /proc/my_config */
static ssize_t proc_config_read(struct file *file, char __user *ubuf,
                                size_t count, loff_t *ppos)
{
    size_t len;

    if (*ppos > 0)
        return 0;

    if (config_len == 0) {
        strncpy(config_buf, "default\n", CONFIG_MAX_LEN - 1);
        config_buf[CONFIG_MAX_LEN - 1] = '\0';
        config_len = strlen(config_buf);
    }

    len = config_len;

    if (count < len)
        return -EINVAL;

    if (copy_to_user(ubuf, config_buf, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

/* Write handler for /proc/my_config */
static ssize_t proc_config_write(struct file *file, const char __user *ubuf,
                                 size_t count, loff_t *ppos)
{
    size_t to_copy;

    if (count == 0)
        return 0;

    to_copy = (count >= (CONFIG_MAX_LEN - 1)) ? (CONFIG_MAX_LEN - 1) : count;

    if (copy_from_user(config_buf, ubuf, to_copy))
        return -EFAULT;

    if (to_copy > 0 && config_buf[to_copy - 1] == '\n')
        to_copy--;

    config_buf[to_copy] = '\n';
    config_buf[to_copy + 1] = '\0';
    config_len = to_copy + 1;

    pr_info("proc_config_module: New value stored (len=%zu)\n", config_len);
    return count;
}

static const struct proc_ops proc_config_ops = {
    .proc_read  = proc_config_read,
    .proc_write = proc_config_write,
};

static int __init proc_config_module_init(void)
{
    proc_config_entry = proc_create(PROC_CONFIG_NAME, 0666, NULL, &proc_config_ops);
    if (!proc_config_entry) {
        pr_err("proc_config_module: Failed to create /proc/%s\n", PROC_CONFIG_NAME);
        return -ENOMEM;
    }

    config_buf[0] = '\0';
    config_len = 0;

    pr_info("proc_config_module: Created /proc/%s (0666)\n", PROC_CONFIG_NAME);
    return 0;
}

static void __exit proc_config_module_exit(void)
{
    if (proc_config_entry) {
        proc_remove(proc_config_entry);
        proc_config_entry = NULL;
        pr_info("proc_config_module: Removed /proc/%s\n", PROC_CONFIG_NAME);
    }
}

module_init(proc_config_module_init);
module_exit(proc_config_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladislav Sokov");
MODULE_DESCRIPTION("/proc/my_config - writable config (Lab 5 Task B)");
MODULE_VERSION("1.0");
