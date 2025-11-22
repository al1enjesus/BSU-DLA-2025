#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define PROC_NAME "my_config"
#define MAX_SIZE 256

static struct proc_dir_entry *proc_file;
static char *config_data;

// Функция чтения (cat /proc/my_config)
static ssize_t proc_read(struct file *file, char __user *ubuf, 
                         size_t count, loff_t *ppos)
{
    int len;

    // Если уже читали - возвращаем 0 (конец файла)
    if (*ppos > 0)
        return 0;

    len = strlen(config_data);

    // Копируем данные из kernel-space в user-space
    if (copy_to_user(ubuf, config_data, len))
        return -EFAULT;

    *ppos = len;
    
    printk(KERN_INFO "my_config: read %d bytes\n", len);
    return len;
}

// Функция записи (echo "text" > /proc/my_config)
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    size_t len = count;

    // Проверка размера
    if (len >= MAX_SIZE)
        len = MAX_SIZE - 1;

    // Очищаем буфер
    memset(config_data, 0, MAX_SIZE);

    // Копируем данные из user-space в kernel-space
    if (copy_from_user(config_data, ubuf, len))
        return -EFAULT;

    // Удаляем \n в конце (если есть)
    if (len > 0 && config_data[len - 1] == '\n')
        config_data[len - 1] = '\0';
    else
        config_data[len] = '\0';

    printk(KERN_INFO "my_config: wrote '%s' (%zu bytes)\n", config_data, len);
    
    return count;
}

// Структура операций для /proc файла
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

// Инициализация модуля
static int __init proc_config_init(void)
{
    // Выделяем память для хранения данных
    config_data = kmalloc(MAX_SIZE, GFP_KERNEL);
    if (!config_data) {
        printk(KERN_ERR "my_config: failed to allocate memory\n");
        return -ENOMEM;
    }

    // Устанавливаем значение по умолчанию
    strcpy(config_data, "default");

    // Создаём /proc файл с правами 0666 (rw-rw-rw-)
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        kfree(config_data);
        printk(KERN_ERR "my_config: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "my_config: module loaded, /proc/%s created\n", PROC_NAME);
    printk(KERN_INFO "my_config: try 'cat /proc/%s' and 'echo text > /proc/%s'\n",
           PROC_NAME, PROC_NAME);
    
    return 0;
}

// Выгрузка модуля
static void __exit proc_config_exit(void)
{
    // Удаляем /proc файл
    proc_remove(proc_file);
    
    // Освобождаем память
    kfree(config_data);

    printk(KERN_INFO "my_config: module unloaded, /proc/%s removed\n", PROC_NAME);
}

module_init(proc_config_init);
module_exit(proc_config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Kurda");
MODULE_DESCRIPTION("Lab5 Task B: /proc file with read/write support");
MODULE_VERSION("1.0");