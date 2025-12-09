#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define PROC_CONFIG_NAME "my_config"
#define CONFIG_MAX_LEN   256U

static struct proc_dir_entry *entry_handle = NULL;
static char stored_data[CONFIG_MAX_LEN];
static size_t current_length = 0U;

/* Обработчик чтения из /proc/my_config */
static ssize_t handle_read(struct file *f, char __user *buffer_from_user,
                           size_t bytes_requested, loff_t *offset)
{
    size_t actual_length;

    if (*offset > 0)
        return 0;

    if (current_length == 0U) {
        (void)strncpy(stored_data, "default\n", CONFIG_MAX_LEN - 1U);
        stored_data[CONFIG_MAX_LEN - 1U] = '\0';
        current_length = strlen(stored_data);
    }

    actual_length = current_length;

    if (bytes_requested < actual_length)
        return -EINVAL;

    if (copy_to_user(buffer_from_user, stored_data, actual_length))
        return -EFAULT;

    *offset = actual_length;
    return (ssize_t)actual_length;
}

/* Обработчик записи в /proc/my_config */
static ssize_t handle_write(struct file *f, const char __user *buffer_from_user,
                            size_t bytes_provided, loff_t *offset)
{
    size_t bytes_to_store;

    if (bytes_provided == 0U)
        return 0;

    bytes_to_store = (bytes_provided >= (CONFIG_MAX_LEN - 1U))
        ? (CONFIG_MAX_LEN - 1U)
        : bytes_provided;

    if (copy_from_user(stored_data, buffer_from_user, bytes_to_store))
        return -EFAULT;

    if (bytes_to_store > 0U && stored_data[bytes_to_store - 1U] == '\n')
        --bytes_to_store;

    stored_data[bytes_to_store]     = '\n';
    stored_data[bytes_to_store + 1U] = '\0';
    current_length = bytes_to_store + 1U;

    pr_info("proc_config_module: Значение обновлено (длина=%zu)\n", current_length);
    return (ssize_t)bytes_provided;
}

static const struct proc_ops config_file_ops = {
    .proc_read  = handle_read,
    .proc_write = handle_write,
};

static int __init module_startup(void)
{
    entry_handle = proc_create(PROC_CONFIG_NAME, 0666, NULL, &config_file_ops);
    if (!entry_handle) {
        pr_err("proc_config_module: Не удалось создать /proc/%s\n", PROC_CONFIG_NAME);
        return -ENOMEM;
    }

    stored_data[0] = '\0';
    current_length = 0U;

    pr_info("proc_config_module: /proc/%s создан (0666)\n", PROC_CONFIG_NAME);
    return 0;
}

static void __exit module_cleanup(void)
{
    if (entry_handle) {
        proc_remove(entry_handle);
        entry_handle = NULL;
        pr_info("proc_config_module: /proc/%s удалён\n", PROC_CONFIG_NAME);
    }
}

module_init(module_startup);
module_exit(module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsareva Anna");
MODULE_DESCRIPTION("/proc/my_config — настраиваемый через procfs параметр (Лаб 5 Задание B)");
MODULE_VERSION("1.0");