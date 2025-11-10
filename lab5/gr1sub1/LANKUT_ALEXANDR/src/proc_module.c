#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MAX_CONFIG_SIZE 256

// Структура для хранения конфигурации
static char config_buffer[MAX_CONFIG_SIZE] = "default";
static size_t config_len = 7;  // strlen("default")
static struct proc_dir_entry *proc_file;

/*
 * Функция чтения /proc/my_config
 */
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    printk(KERN_INFO "proc_module: read called\n");

    // Если уже читали, возвращаем 0
    if (*ppos > 0)
        return 0;

    // Проверяем, хватает ли места
    if (count < config_len)
        return -EFAULT;

    // Копируем данные в user-space
    if (copy_to_user(ubuf, config_buffer, config_len)) {
        printk(KERN_ERR "proc_module: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos = config_len;
    return config_len;
}

/*
 * Функция записи /proc/my_config
 */
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    size_t to_write;

    printk(KERN_INFO "proc_module: write called (count=%zu)\n", count);

    // Проверяем максимальный размер
    if (count > MAX_CONFIG_SIZE) {
        printk(KERN_WARNING "proc_module: write size %zu exceeds MAX %d\n",
               count, MAX_CONFIG_SIZE);
        to_write = MAX_CONFIG_SIZE;
    } else {
        to_write = count;
    }

    // Копируем данные из user-space в kernel-space
    if (copy_from_user(config_buffer, ubuf, to_write)) {
        printk(KERN_ERR "proc_module: copy_from_user failed\n");
        return -EFAULT;
    }

    // Удаляем переновод строки, если есть
    if (to_write > 0 && config_buffer[to_write - 1] == '\n') {
        to_write--;
    }

    config_len = to_write;
    config_buffer[config_len] = '\0';

    printk(KERN_INFO "proc_module: config updated: %.*s\n",
           (int)config_len, config_buffer);

    return count;  // Возвращаем количество написанных байт
}

// Структура file_operations для /proc файла
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

/*
 * Функция инициализации модуля
 */
static int __init proc_init(void)
{
    printk(KERN_INFO "proc_module: Initializing...\n");

    // Создаём /proc файл с правами 0666 (читать/писать всем)
    // Параметры: имя, права доступа, родительская папка, file_ops
    proc_file = proc_create("my_config", 0666, NULL, &proc_fops);

    if (!proc_file) {
        printk(KERN_ERR "proc_module: Failed to create /proc/my_config\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_module: /proc/my_config created successfully\n");
    printk(KERN_INFO "proc_module: Initial value: %s\n", config_buffer);
    return 0;
}

/*
 * Функция выгрузки модуля
 */
static void __exit proc_exit(void)
{
    // Удаляем /proc файл
    proc_remove(proc_file);

    printk(KERN_INFO "proc_module: /proc/my_config removed\n");
    printk(KERN_INFO "proc_module: Final value was: %s\n", config_buffer);
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ланкуть Александр");
MODULE_DESCRIPTION("Kernel module with /proc file with read/write support");
MODULE_VERSION("1.0");
