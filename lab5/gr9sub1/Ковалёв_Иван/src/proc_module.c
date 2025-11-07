#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("/proc/student_info module with statistics");
MODULE_VERSION("1.0");

// Глобальные переменные
static unsigned long module_load_time;
static int read_count = 0;

// Для современных версий ядра используем proc_ops
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#define HAVE_PROC_OPS
#endif

// Функция чтения из /proc файла
static ssize_t student_info_read(struct file *file, char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    char buffer[512];
    int len;
    
    // Если уже прочитали весь файл, возвращаем 0
    if (*ppos > 0)
        return 0;
    
    // Увеличиваем счетчик обращений
    read_count++;
    
    // Формируем строку с информацией
    len = snprintf(buffer, sizeof(buffer),
                  "Name: Ковалёв Иван\n"  // Замените на ваше имя
                  "Group: 9, Subgroup: 1\n"   // Замените на ваши данные
                  "Module loaded at: %lu jiffies\n"
                  "Read count: %d\n\n",
                  module_load_time, read_count);
    
    // Копируем данные из kernel-space в user-space
    if (copy_to_user(user_buf, buffer, len)) {
        return -EFAULT;  // Ошибка копирования
    }
    
    *ppos = len;  // Обновляем позицию
    return len;    // Возвращаем количество прочитанных байт
}

// Определяем операции с файлом
#ifdef HAVE_PROC_OPS
static const struct proc_ops student_info_fops = {
    .proc_read = student_info_read,
};
#else
static const struct file_operations student_info_fops = {
    .read = student_info_read,
};
#endif

// Указатель на proc файл
static struct proc_dir_entry *proc_file;

// Функция инициализации модуля
static int __init proc_module_init(void)
{
    // Сохраняем время загрузки модуля
    module_load_time = jiffies;
    
    // Создаем файл в /proc
    proc_file = proc_create("student_info", 0444, NULL, &student_info_fops);
    
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc/student_info\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "proc_module: /proc/student_info created\n");
    printk(KERN_INFO "proc_module: Loaded at %lu jiffies\n", module_load_time);
    
    return 0;
}

// Функция выгрузки модуля
static void __exit proc_module_exit(void)
{
    // Удаляем proc файл
    if (proc_file)
        proc_remove(proc_file);
    
    printk(KERN_INFO "proc_module: /proc/student_info removed\n");
    printk(KERN_INFO "proc_module: Final read count: %d\n", read_count);
}

// Регистрация функций
module_init(proc_module_init);
module_exit(proc_module_exit);
