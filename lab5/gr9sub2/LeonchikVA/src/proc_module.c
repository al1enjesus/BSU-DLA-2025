#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#define PROC_NAME "student_info"
#define BUF_SIZE 512

// Глобальные переменные
static struct proc_dir_entry *proc_file;
static unsigned long load_time;      // Время загрузки модуля в jiffies
static unsigned int read_count = 0;   // Счётчик обращений к файлу

// Функция чтения из /proc файла
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos) {
    char buf[BUF_SIZE];
    int len;

    // Если уже читали, возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // Увеличиваем счётчик обращений
    read_count++;

    // Формируем вывод с информацией о студенте
    len = snprintf(buf, sizeof(buf),
                   "Name: Leonchik Vladislav\n"
                   "Group: 9, Subgroup: 2\n"
                   "Module loaded at: %lu jiffies\n"
                   "Read count: %u\n",
                   load_time, read_count);

    // Копируем данные из kernel space в user space
    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    // Обновляем позицию в файле
    *ppos = len;

    printk(KERN_INFO "proc_module: /proc/%s read (count: %u)\n", 
           PROC_NAME, read_count);

    return len;
}

// Структура операций для /proc файла
static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
};

// Функция инициализации модуля
static int __init proc_module_init(void) {
    // Сохраняем время загрузки модуля
    load_time = jiffies;

    // Создаём /proc файл
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    if (!proc_file) {
        printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_module: Module loaded, /proc/%s created at %lu jiffies\n",
           PROC_NAME, load_time);

    return 0;
}

// Функция выгрузки модуля
static void __exit proc_module_exit(void) {
    // Удаляем /proc файл
    proc_remove(proc_file);

    printk(KERN_INFO "proc_module: Module unloaded, /proc/%s removed\n", PROC_NAME);
    printk(KERN_INFO "proc_module: Total reads: %u\n", read_count);
}

// Регистрация функций init/exit
module_init(proc_module_init);
module_exit(proc_module_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonchik Vladislav");
MODULE_DESCRIPTION("Kernel module with /proc interface for student information");
MODULE_VERSION("1.0");