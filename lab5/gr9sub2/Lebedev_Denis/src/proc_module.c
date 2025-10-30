#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define PROC_FILENAME "my_config"
#define MAX_BUFFER_SIZE 256

static struct proc_dir_entry *proc_file;
static char *proc_buffer;
static DEFINE_MUTEX(proc_mutex);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lebedev Denis");
MODULE_DESCRIPTION("Task B: /proc file with read/write support (thread-safe).");
MODULE_VERSION("1.1");

/**
 * proc_read - Обработчик чтения из /proc/my_config
 * Копирует содержимое буфера в user-space
 */
static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    int len;
    ssize_t ret;

    if (*ppos > 0)
        return 0;

    /* Защита от одновременного доступа к буферу */
    mutex_lock(&proc_mutex);   
    len = strlen(proc_buffer);

    if (copy_to_user(ubuf, proc_buffer, len)) {
        mutex_unlock(&proc_mutex);
        printk(KERN_ERR "proc_module: copy_to_user failed.\n");
        return -EFAULT;
    }

    *ppos = len;
    ret = len;

    mutex_unlock(&proc_mutex);

    return ret;
}

/**
 * proc_write - Обработчик записи в /proc/my_config
 * Копирует данные из user-space в буфер модуля
 */
static ssize_t proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    if (count > MAX_BUFFER_SIZE - 1) {
        printk(KERN_WARNING "proc_module: Input is too long. Truncating to %d bytes.\n", MAX_BUFFER_SIZE - 1);
        count = MAX_BUFFER_SIZE - 1;
    }

    mutex_lock(&proc_mutex);

    if (copy_from_user(proc_buffer, ubuf, count)) {
        mutex_unlock(&proc_mutex);
        printk(KERN_ERR "proc_module: copy_from_user failed.\n");
        return -EFAULT;
    }

    proc_buffer[count] = '\0';

    /* Удаляем символ новой строки, если он есть (echo добавляет \n) */
    if (count > 0 && proc_buffer[count - 1] == '\n')
        proc_buffer[count - 1] = '\0';

    *ppos = count;

    mutex_unlock(&proc_mutex);

    return count;
}

static const struct proc_ops my_proc_ops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

/**
 * proc_init - Функция инициализации модуля
 * Выделяет память и создаёт файл /proc/my_config
 */
static int __init proc_init(void)
{
    proc_buffer = kmalloc(MAX_BUFFER_SIZE, GFP_KERNEL);
    if (!proc_buffer) {
        printk(KERN_ERR "proc_module: Failed to allocate memory.\n");
        return -ENOMEM;
    }
    strscpy(proc_buffer, "default", MAX_BUFFER_SIZE);

    proc_file = proc_create(PROC_FILENAME, 0666, NULL, &my_proc_ops);
    if (proc_file == NULL) {
        kfree(proc_buffer);
        printk(KERN_ERR "proc_module: Failed to create /proc/%s.\n", PROC_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_module: /proc/%s created.\n", PROC_FILENAME);
    return 0;
}

/**
 * proc_exit - Функция выгрузки модуля
 * Удаляет файл /proc/my_config и освобождает память
 */
static void __exit proc_exit(void)
{
    proc_remove(proc_file);
    kfree(proc_buffer);
    printk(KERN_INFO "proc_module: /proc/%s removed.\n", PROC_FILENAME);
}

module_init(proc_init);
module_exit(proc_exit);
