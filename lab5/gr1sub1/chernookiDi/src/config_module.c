/*
 * config_module.c - Модуль с /proc файлом для чтения и записи
 * 
 * Автор: Черноокий Д И
 * Номер студента: 20 (Вариант 2 - чётный номер)
 * Задание B: /proc файл со статусом на чтение/запись
 *
 * Функциональность:
 * - Создаёт файл /proc/my_config
 * - По умолчанию содержит строку "default"
 * - При записи (echo "text" > /proc/my_config) сохраняет новое значение
 * - При чтении (cat /proc/my_config) возвращает текущее значение
 * - Максимальная длина строки: 256 символов
 *
 * Компиляция: make
 * Загрузка: sudo insmod config_module.ko
 * Использование:
 *   cat /proc/my_config       # Просмотр текущего значения
 *   echo "new text" > /proc/my_config  # Запись нового значения
 * Выгрузка: sudo rmmod config_module
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <string.h>

#define MAX_CONFIG_SIZE 256

/* Защита от одновременного доступа */
static DEFINE_MUTEX(config_mutex);

/* Статическое хранилище конфигурации */
static char config_data[MAX_CONFIG_SIZE] = "default";
static size_t config_len = 7; /* strlen("default") */

static struct proc_dir_entry *proc_file = NULL;

/*
 * Функция чтения файла /proc/my_config
 * Вызывается при: cat /proc/my_config
 */
static ssize_t proc_config_read(struct file *file, char __user *ubuf,
                                size_t count, loff_t *ppos)
{
    ssize_t ret;
    
    /* Если уже прочитали всё, вернуть 0 */
    if (*ppos >= config_len)
        return 0;
    
    /* Получить мьютекс для безопасного доступа */
    mutex_lock(&config_mutex);
    
    /* Количество байт для чтения */
    if (count > config_len - *ppos)
        count = config_len - *ppos;
    
    /* Копировать данные из kernel-space в user-space */
    if (copy_to_user(ubuf, config_data + *ppos, count)) {
        mutex_unlock(&config_mutex);
        return -EFAULT;
    }
    
    *ppos += count;
    ret = count;
    
    mutex_unlock(&config_mutex);
    
    return ret;
}

/*
 * Функция записи файла /proc/my_config
 * Вызывается при: echo "text" > /proc/my_config
 */
static ssize_t proc_config_write(struct file *file, const char __user *ubuf,
                                 size_t count, loff_t *ppos)
{
    /* Проверить размер */
    if (count >= MAX_CONFIG_SIZE)
        count = MAX_CONFIG_SIZE - 1;
    
    /* Получить мьютекс */
    mutex_lock(&config_mutex);
    
    /* Копировать данные из user-space в kernel-space */
    if (copy_from_user(config_data, ubuf, count)) {
        mutex_unlock(&config_mutex);
        return -EFAULT;
    }
    
    config_data[count] = '\0'; /* Завершить строку */
    config_len = count;
    
    printk(KERN_INFO "config_module: Configuration updated to: %s\n", config_data);
    
    mutex_unlock(&config_mutex);
    
    return count;
}

/* Операции над /proc файлом */
static const struct proc_ops proc_config_ops = {
    .proc_read = proc_config_read,
    .proc_write = proc_config_write,
};

/*
 * Инициализация модуля
 */
static int __init config_init(void)
{
    /* Создать /proc файл с правами 0666 (read-write для всех) */
    proc_file = proc_create("my_config", 0666, NULL, &proc_config_ops);
    
    if (!proc_file) {
        printk(KERN_ERR "config_module: Failed to create /proc/my_config\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "config_module: Module loaded successfully\n");
    printk(KERN_INFO "config_module: /proc/my_config created\n");
    
    return 0;
}

/*
 * Выгрузка модуля
 */
static void __exit config_exit(void)
{
    if (proc_file) {
        proc_remove(proc_file);
    }
    
    printk(KERN_INFO "config_module: Module unloaded\n");
}

/* Регистрация функций init/exit */
module_init(config_init);
module_exit(config_exit);

/* Метаданные модуля */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernel module with /proc file for reading and writing configuration");
MODULE_VERSION("1.0");
