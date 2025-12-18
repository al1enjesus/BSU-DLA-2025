/*
 * proc_module.c - Модуль с /proc файлом
 *
 * Создаёт файл /proc/student_info с информацией о студенте
 * и счётчиком обращений.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#define PROC_NAME "student_info"
#define MAX_SIZE 1024

// Глобальные переменные
static struct proc_dir_entry *proc_file = NULL;
static int read_count = 0;
static unsigned long load_time = 0;

// Функция чтения из /proc файла
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[MAX_SIZE];
    int len;

    // Если уже читали (повторный вызов), возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Увеличиваем счётчик обращений
    read_count++;

    // Заполняем buf информацией
    len = snprintf(buf, sizeof(buf),
        "Name: Titkova Anna\n"
        "Group: 9, Subgroup: 1\n"
        "Module loaded at: %lu jiffies\n"
        "Read count: %d\n",
        load_time, read_count);

    // Скопируйте данные из kernel space в user space
    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    // Обновляем позицию чтения
    *ppos = len;

    return len;
}

// Структура операций для proc файла
static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
};

// Функция инициализации модуля
static int __init proc_module_init(void)
{
    printk(KERN_INFO "proc_module: Initializing\n");

    // Сохраняем текущее время загрузки
    load_time = jiffies;

    // Создаем proc файл
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    if (!proc_file) {
        printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_module: Created /proc/%s\n", PROC_NAME);

    return 0;
}

// Функция выгрузки модуля
static void __exit proc_module_exit(void)
{
    // Удаляем proc файл
    if (proc_file) {
        proc_remove(proc_file);
        printk(KERN_INFO "proc_module: Removed /proc/%s\n", PROC_NAME);
    }
}

module_init(proc_module_init);
module_exit(proc_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Titkova Anna");
MODULE_DESCRIPTION("Proc filesystem example");
MODULE_VERSION("1.0");
