/*
 * stats_module.c - Модуль с /proc файлом со статистикой системы
 * 
 * Автор: Черноокий Д И
 * Номер студента: 20 (Вариант 2 - чётный номер)
 * Задание C: /proc файл со статистикой системы
 *
 * Функциональность:
 * - Создаёт файл /proc/sys_stats
 * - Выводит информацию о системе:
 *   * Количество запущенных процессов
 *   * Используемая память (в MB)
 *   * Uptime системы (в секундах)
 *   * Загрузка процессора (load average)
 *
 * Компиляция: make
 * Загрузка: sudo insmod stats_module.ko
 * Использование:
 *   cat /proc/sys_stats       # Просмотр статистики
 * Выгрузка: sudo rmmod stats_module
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sysinfo.h>
#include <linux/jiffies.h>
#include <linux/avenrun.h>

#define BUFFER_SIZE 1024

static struct proc_dir_entry *proc_file = NULL;

/* Функция подсчета количества процессов */
static int count_processes(void)
{
    struct task_struct *p;
    int count = 0;
    
    /* Перебрать все процессы в системе */
    for_each_process(p) {
        count++;
    }
    
    return count;
}

/*
 * Функция чтения файла /proc/sys_stats
 * Вызывается при: cat /proc/sys_stats
 */
static ssize_t proc_stats_read(struct file *file, char __user *ubuf,
                               size_t count, loff_t *ppos)
{
    ssize_t ret;
    char *buf;
    struct sysinfo si;
    int processes;
    unsigned long uptime_sec;
    unsigned long load_avg;
    
    /* Выделить буфер для ответа */
    buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    
    /* Если уже прочитали, вернуть 0 */
    if (*ppos > 0) {
        kfree(buf);
        return 0;
    }
    
    /* Получить информацию о системе */
    si_meminfo(&si);
    processes = count_processes();
    uptime_sec = jiffies_to_msecs(jiffies) / 1000;
    
    /* Load average из avenrun (в сотых долях на 1 процессор) */
    load_avg = avenrun[0];
    
    /* Вычислить реально используемую память */
    unsigned long mem_used = (si.totalram - si.freeram) * si.mem_unit / (1024 * 1024);
    unsigned long mem_total = si.totalram * si.mem_unit / (1024 * 1024);
    
    /* Форматировать вывод в буфер */
    int len = snprintf(buf, BUFFER_SIZE,
        "=== System Statistics ===\n"
        "Processes running: %d\n"
        "Memory Used: %lu MB\n"
        "Memory Total: %lu MB\n"
        "Memory Free: %lu MB\n"
        "System Uptime: %lu seconds\n"
        "Load Average: %lu.%02lu\n"
        "================================\n",
        processes,
        mem_used,
        mem_total,
        si.freeram * si.mem_unit / (1024 * 1024),
        uptime_sec,
        load_avg / FIXED_1,
        (load_avg % FIXED_1) * 100 / FIXED_1
    );
    
    /* Проверка размера буфера */
    if (len >= BUFFER_SIZE) {
        len = BUFFER_SIZE - 1;
    }
    
    /* Ограничить количество для чтения */
    if (count > len)
        count = len;
    
    /* Копировать данные в user-space */
    if (copy_to_user(ubuf, buf, count)) {
        kfree(buf);
        return -EFAULT;
    }
    
    *ppos = count;
    ret = count;
    
    kfree(buf);
    
    return ret;
}

/* Операции над /proc файлом */
static const struct proc_ops proc_stats_ops = {
    .proc_read = proc_stats_read,
};

/*
 * Инициализация модуля
 */
static int __init stats_init(void)
{
    /* Создать /proc файл с правами 0444 (read-only) */
    proc_file = proc_create("sys_stats", 0444, NULL, &proc_stats_ops);
    
    if (!proc_file) {
        printk(KERN_ERR "stats_module: Failed to create /proc/sys_stats\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "stats_module: Module loaded successfully\n");
    printk(KERN_INFO "stats_module: /proc/sys_stats created\n");
    
    return 0;
}

/*
 * Выгрузка модуля
 */
static void __exit stats_exit(void)
{
    if (proc_file) {
        proc_remove(proc_file);
    }
    
    printk(KERN_INFO "stats_module: Module unloaded\n");
}

/* Регистрация функций init/exit */
module_init(stats_init);
module_exit(stats_exit);

/* Метаданные модуля */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chernookii D.I. <chernookii@bsu.by>");
MODULE_DESCRIPTION("Kernel module that provides system statistics via /proc");
MODULE_VERSION("1.0");
