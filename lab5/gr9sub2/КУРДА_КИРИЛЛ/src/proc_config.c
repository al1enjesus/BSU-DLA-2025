#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>

#define PROC_NAME "my_config"
#define MAX_SIZE 256 /* объяснение: буфер для небольшой конфигурации; достаточно для задания */

static struct proc_dir_entry *proc_file;
static char *config_data;
static DEFINE_MUTEX(config_lock);

/* чтение: поддерживает частичное чтение */
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    size_t len, remain, to_copy;
    ssize_t ret = 0;

    if (!config_data)
        return -EIO;

    mutex_lock(&config_lock);

    len = strnlen(config_data, MAX_SIZE);

    /* если смещение за пределами — EOF */
    if ((size_t)*ppos >= len) {
        ret = 0;
        goto out;
    }

    remain = len - (size_t)*ppos;
    to_copy = (remain < count) ? remain : count;

    if (copy_to_user(ubuf, config_data + *ppos, to_copy)) {
        ret = -EFAULT;
        goto out;
    }

    *ppos += to_copy;
    ret = to_copy;

    printk(KERN_INFO "my_config: read %zu bytes (pos=%lld)\n", to_copy, *ppos);

out:
    mutex_unlock(&config_lock);
    return ret;
}

/* запись: защищено мьютексом, проверяется длина и copy_from_user */
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    size_t len = (count < (MAX_SIZE - 1)) ? count : (MAX_SIZE - 1);

    if (!config_data)
        return -EIO;

    /* Записываем с блокировкой */
    mutex_lock(&config_lock);

    /* Обнулим буфер, чтобы не оставлять "хвост" старых данных */
    memset(config_data, 0, MAX_SIZE);

    if (copy_from_user(config_data, ubuf, len)) {
        mutex_unlock(&config_lock);
        return -EFAULT;
    }

    /* Убрать завершающий '\n' если есть */
    if (len > 0 && config_data[len - 1] == '\n')
        config_data[len - 1] = '\0';
    else
        config_data[len] = '\0';

    printk(KERN_INFO "my_config: wrote '%s' (%zu bytes)\n", config_data, len);

    mutex_unlock(&config_lock);

    /* В соответствии с поведением echo > file возвращаем count (кол-во переданных байт) */
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read  = proc_read,
    .proc_write = proc_write,
};

static int __init proc_config_init(void)
{
    /* Выделяем ноль-инициализированный буфер */
    config_data = kzalloc(MAX_SIZE, GFP_KERNEL);
    if (!config_data) {
        printk(KERN_ERR "my_config: failed to allocate memory (%d bytes)\n", MAX_SIZE);
        return -ENOMEM;
    }

    /* Устанавливаем значение по умолчанию */
    strlcpy(config_data, "default", MAX_SIZE);

    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "my_config: failed to create /proc/%s\n", PROC_NAME);
        kfree(config_data);
        config_data = NULL;
        return -ENOMEM;
    }

    printk(KERN_INFO "my_config: module loaded, /proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit proc_config_exit(void)
{
    if (proc_file) {
        proc_remove(proc_file);
        proc_file = NULL;
    }

    kfree(config_data);
    config_data = NULL;

    printk(KERN_INFO "my_config: module unloaded, /proc/%s removed\n", PROC_NAME);
}

module_init(proc_config_init);
module_exit(proc_config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Kurda");
MODULE_DESCRIPTION("Lab5 Task B: /proc file with read/write support (hardened)");
MODULE_VERSION("1.1");
