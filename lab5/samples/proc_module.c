/*
 * proc_module.c - Модуль с /proc файлом
 *
 * Создаёт файл /proc/student_info с информацией о студенте
 * и счётчиком обращений.
 *
 * Компиляция: make
 * Использование:
 *   sudo insmod proc_module.ko
 *   cat /proc/student_info
 *   sudo rmmod proc_module
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

#define PROC_NAME "student_info"
#define MAX_SIZE 1024

// TODO: Объявите глобальные переменные:
// - Указатель на proc_dir_entry (для хранения proc файла)
// - Счётчик обращений (int read_count = 0)
// - Время загрузки модуля (unsigned long load_time)

static struct proc_dir_entry *proc_file = NULL;
static int read_count = 0;
static unsigned long load_time = 0;

// TODO: Реализуйте функцию чтения из /proc файла
// Эта функция вызывается когда кто-то делает: cat /proc/student_info
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[MAX_SIZE];
    int len;

    // Если уже читали (повторный вызов), возвращаем 0 (EOF)
    if (*ppos > 0)
        return 0;

    // TODO: Увеличьте счётчик обращений
    // read_count++;

    // TODO: Заполните buf информацией:
    // - Ваше имя (например: "Name: Ivan Ivanov")
    // - Группа и подгруппа (например: "Group: 6, Subgroup: 1")
    // - Время загрузки модуля в jiffies (например: "Module loaded at: 12345 jiffies")
    // - Счётчик обращений (например: "Read count: 3")
    //
    // Используйте snprintf для форматирования
    //
    // Пример:
    // len = snprintf(buf, sizeof(buf),
    //     "Name: %s\n"
    //     "Group: %d, Subgroup: %d\n"
    //     "Module loaded at: %lu jiffies\n"
    //     "Read count: %d\n",
    //     "Ivan Ivanov", 6, 1, load_time, read_count);

    len = snprintf(buf, sizeof(buf),
        "TODO: Fill with your information\n"
        "Read count: %d\n", read_count);

    // TODO: Скопируйте данные из kernel space в user space
    // Используйте copy_to_user(ubuf, buf, len)
    // Проверьте возвращаемое значение (если != 0, значит ошибка)
    //
    // if (copy_to_user(ubuf, buf, len))
    //     return -EFAULT;

    // Обновляем позицию чтения
    *ppos = len;

    return len;
}

// Структура операций для proc файла
static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
};

// TODO: Функция инициализации модуля
static int __init proc_module_init(void)
{
    printk(KERN_INFO "proc_module: Initializing\n");

    // TODO: Сохраните текущее время загрузки
    // load_time = jiffies;

    // TODO: Создайте proc файл
    // proc_create() создаёт файл в /proc
    // Параметры: имя, права доступа, родительская директория, операции
    //
    // proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    // if (!proc_file) {
    //     printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
    //     return -ENOMEM;
    // }

    printk(KERN_INFO "proc_module: Created /proc/%s (TODO: implement)\n", PROC_NAME);

    return 0;
}

// TODO: Функция выгрузки модуля
static void __exit proc_module_exit(void)
{
    // TODO: Удалите proc файл
    // Используйте proc_remove(proc_file);
    //
    // if (proc_file) {
    //     proc_remove(proc_file);
    //     printk(KERN_INFO "proc_module: Removed /proc/%s\n", PROC_NAME);
    // }

    printk(KERN_INFO "proc_module: Exiting (TODO: remove proc file)\n");
}

module_init(proc_module_init);
module_exit(proc_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");  // TODO: Ваше имя
MODULE_DESCRIPTION("Proc filesystem example");
MODULE_VERSION("1.0");

/*
 * ЗАДАНИЯ для студента:
 *
 * 1. Реализуйте все TODO в коде выше
 *
 * 2. В proc_read():
 *    - Увеличьте счётчик обращений
 *    - Заполните buf информацией о себе
 *    - Используйте copy_to_user для копирования в user space
 *
 * 3. В proc_module_init():
 *    - Сохраните текущие jiffies
 *    - Создайте /proc файл с помощью proc_create()
 *    - Проверьте на ошибки
 *
 * 4. В proc_module_exit():
 *    - Удалите /proc файл с помощью proc_remove()
 *
 * 5. Протестируйте:
 *    $ make
 *    $ sudo insmod proc_module.ko
 *    $ cat /proc/student_info
 *    $ cat /proc/student_info
 *    $ cat /proc/student_info
 *    $ sudo rmmod proc_module
 *    $ dmesg | tail -10
 *
 * ОЖИДАЕМЫЙ РЕЗУЛЬТАТ:
 *
 * $ cat /proc/student_info
 * Name: Ivan Ivanov
 * Group: 6, Subgroup: 1
 * Module loaded at: 4295123456 jiffies
 * Read count: 1
 *
 * $ cat /proc/student_info
 * Name: Ivan Ivanov
 * Group: 6, Subgroup: 1
 * Module loaded at: 4295123456 jiffies
 * Read count: 2
 *
 * ОТЛАДКА:
 *
 * Если cat /proc/student_info выдаёт ошибку:
 * 1. Проверьте dmesg на наличие ошибок
 * 2. Проверьте, создался ли файл: ls -la /proc/student_info
 * 3. Убедитесь, что proc_create вернула не-NULL
 *
 * Если данные не обновляются:
 * 1. Проверьте, увеличивается ли read_count
 * 2. Убедитесь, что *ppos правильно обрабатывается
 *
 * ДОПОЛНИТЕЛЬНО (*):
 *
 * - Добавьте uptime в секундах (используйте jiffies_to_msecs)
 * - Добавьте текущее время (используйте ktime_get_real_seconds)
 * - Форматируйте вывод более красиво (ASCII табличка)
 */
