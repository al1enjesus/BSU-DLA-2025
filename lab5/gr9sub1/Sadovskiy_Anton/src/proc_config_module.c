
/*
 * proc_config_module.c - Задание B: /proc файл с записью
 *
 * Создаёт файл /proc/my_config с возможностью чтения и записи:
 * - По умолчанию содержит "default"
 * - При записи сохраняет новое значение (максимум 256 символов)
 * - При чтении возвращает текущее значение
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h> // copy_to_user, copy_from_user
#include <linux/slab.h>    // kmalloc, kfree

#define PROC_NAME "my_config"
#define MAX_CONFIG_LEN 256

// Глобальные переменные
static struct proc_dir_entry *proc_file;
static char config_buffer[MAX_CONFIG_LEN] = "default";
static size_t config_len = 7; // Длина "default"

// Функция чтения - вызывается при cat /proc/my_config
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char output_buf[MAX_CONFIG_LEN + 2]; // +2 для \n и \0
    int len;

    // Если уже читали - возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Формируем вывод с переводом строки
    len = snprintf(output_buf, sizeof(output_buf), "%s\n", config_buffer);

    // Проверяем, что запрошено достаточно байт
    if (count < len)
        len = count;

    // Копируем данные в user-space
    if (copy_to_user(ubuf, output_buf, len))
    {
        printk(KERN_ERR "proc_config: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos = len;

    printk(KERN_INFO "proc_config: Read '%s' (%d bytes)\n", config_buffer, len);
    return len;
}

// Функция записи - вызывается при echo "text" > /proc/my_config
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    char temp_buf[MAX_CONFIG_LEN];
    size_t len;

    // Ограничиваем размер записи
    len = count;
    if (len >= MAX_CONFIG_LEN)
        len = MAX_CONFIG_LEN - 1;

    // Копируем данные из user-space
    if (copy_from_user(temp_buf, ubuf, len))
    {
        printk(KERN_ERR "proc_config: copy_from_user failed\n");
        return -EFAULT;
    }

    // Убираем символ новой строки в конце (если есть)
    if (len > 0 && temp_buf[len - 1] == '\n')
        len--;

    // Завершаем строку нулевым символом
    temp_buf[len] = '\0';

    // Сохраняем новое значение
    strncpy(config_buffer, temp_buf, MAX_CONFIG_LEN - 1);
    config_buffer[MAX_CONFIG_LEN - 1] = '\0';
    config_len = len;

    printk(KERN_INFO "proc_config: Wrote '%s' (%zu bytes)\n", config_buffer, len);

    return count; // Возвращаем исходное количество байт
}

// Структура операций для proc файла
static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

// Инициализация модуля
static int __init proc_config_init(void)
{
    // Создаём файл /proc/my_config с правами 0666 (rw-rw-rw-)
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_file_ops);

    if (!proc_file)
    {
        printk(KERN_ERR "proc_config: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_config: Module loaded, created /proc/%s\n", PROC_NAME);
    printk(KERN_INFO "proc_config: Default value: '%s'\n", config_buffer);

    return 0;
}

// Выгрузка модуля
static void __exit proc_config_exit(void)
{
    // Удаляем proc файл
    proc_remove(proc_file);

    printk(KERN_INFO "proc_config: Module unloaded, removed /proc/%s\n", PROC_NAME);
    printk(KERN_INFO "proc_config: Final value was: '%s'\n", config_buffer);
}

module_init(proc_config_init);
module_exit(proc_config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Sadovskiy anton.crykov@gmail.com");
MODULE_DESCRIPTION("/proc file with read/write support for configuration");
MODULE_VERSION("1.0");
