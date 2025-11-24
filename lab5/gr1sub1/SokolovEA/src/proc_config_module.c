#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define PROC_NAME "my_config"
#define MAX_SIZE 256

// Глобальные переменные
static struct proc_dir_entry *proc_file = NULL;
static char *config_data = NULL;
static size_t config_len = 0;

// Функция чтения из /proc файла - вызывается при "cat /proc/my_config"
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    // Если уже читали (повторный вызов), возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Копируем данные в user-space
    if (copy_to_user(ubuf, config_data, config_len))
        return -EFAULT;

    *ppos = config_len;
    return config_len;
}

// Функция записи в /proc файл - вызывается при "echo ... > /proc/my_config" 
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    char *temp_buffer;

    // Ограничиваем размер записи
    if (count > MAX_SIZE)
        count = MAX_SIZE;

    // Выделяем временный буфер
    temp_buffer = kmalloc(count + 1, GFP_KERNEL);
    if (!temp_buffer)
        return -ENOMEM;

    // Копируем данные из user-space
    if (copy_from_user(temp_buffer, ubuf, count)) {
        kfree(temp_buffer);
        return -EFAULT;
    }

    // Убираем символ новой строки если есть
    if (count > 0 && temp_buffer[count-1] == '\n') {
        temp_buffer[count-1] = '\0';
        count--;
    } else {
        temp_buffer[count] = '\0';
    }

    // Освобождаем старые данные и сохраняем новые
    kfree(config_data);
    config_data = temp_buffer;
    config_len = count;

    printk(KERN_INFO "proc_config: New value set: %s\n", config_data);
    return count;
}

// Структура операций для proc файла
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

// Функция инициализации модуля
static int __init proc_config_init(void)
{
    // Выделяем память для начального значения "default"
    config_data = kmalloc(8, GFP_KERNEL);
    if (!config_data) {
        printk(KERN_ERR "proc_config: Failed to allocate memory\n");
        return -ENOMEM;
    }
    strcpy(config_data, "default");
    config_len = 7;

    // Создаём proc файл с правами на чтение и запись
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "proc_config: Failed to create /proc/%s\n", PROC_NAME);
        kfree(config_data);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_config: Module loaded, /proc/%s created\n", PROC_NAME);
    return 0;
}

// Функция выгрузки модуля
static void __exit proc_config_exit(void)
{
    // Удаляем proc файл
    if (proc_file) {
        proc_remove(proc_file);
        printk(KERN_INFO "proc_config: /proc/%s removed\n", PROC_NAME);
    }

    // Освобождаем память
    if (config_data) {
        kfree(config_data);
        printk(KERN_INFO "proc_config: Memory freed\n");
    }

    printk(KERN_INFO "proc_config: Module unloaded\n");
}

module_init(proc_config_init);
module_exit(proc_config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sokolov Evgeny");
MODULE_DESCRIPTION("Proc config module with read/write capability");
MODULE_VERSION("1.0");