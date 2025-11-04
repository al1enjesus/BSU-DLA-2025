/*
 * hello_module.c - Простейший модуль ядра "Hello World"
 *
 * Компиляция: make
 * Загрузка: sudo insmod hello_module.ko
 * Выгрузка: sudo rmmod hello_module
 * Логи: dmesg | tail
 *
 * Это СКЕЛЕТ - реализуйте TODO самостоятельно!
 */

#include <linux/module.h>    // Обязательно для всех модулей
#include <linux/kernel.h>    // Для printk, KERN_*
#include <linux/init.h>      // Для __init, __exit
#include <linux/moduleparam.h>  // Для module_param

// TODO: Добавьте параметр модуля "message" (строка)
// По умолчанию должен быть NULL
// Подсказка: static char *message = NULL;
//            module_param(message, charp, 0644);
//            MODULE_PARM_DESC(message, "Custom greeting message");

// TODO: Функция инициализации модуля
// Вызывается при insmod
static int __init hello_init(void)
{
    // TODO: Если параметр message задан, вывести его
    // Иначе вывести "Hello from [ВАШ_ИМЯ] module!"
    //
    // Подсказка:
    // if (message) {
    //     printk(KERN_INFO "hello_module: %s\n", message);
    // } else {
    //     printk(KERN_INFO "hello_module: Hello from [ВАШ_ИМЯ] module!\n");
    // }

    printk(KERN_INFO "hello_module: Module loaded (TODO: implement greeting)\n");

    return 0;  // 0 = успех
}

// TODO: Функция выгрузки модуля
// Вызывается при rmmod
static void __exit hello_exit(void)
{
    // TODO: Вывести "Goodbye from [ВАШ_ИМЯ] module!"
    //
    // printk(KERN_INFO "hello_module: Goodbye from [ВАШ_ИМЯ] module!\n");

    printk(KERN_INFO "hello_module: Module unloaded (TODO: implement goodbye)\n");
}

// Регистрация функций init/exit
module_init(hello_init);
module_exit(hello_exit);

// TODO: Заполните метаданные
MODULE_LICENSE("GPL");                    // ОБЯЗАТЕЛЬНО!
MODULE_AUTHOR("Your Name <your@email>");  // TODO: Ваше имя
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");

/*
 * ЗАДАНИЯ для студента:
 *
 * 1. Заполните TODO в коде выше
 *
 * 2. Добавьте параметр модуля "message"
 *
 * 3. В hello_init():
 *    - Если message != NULL, вывести его
 *    - Иначе вывести дефолтное приветствие с вашим именем
 *
 * 4. В hello_exit():
 *    - Вывести прощание с вашим именем
 *
 * 5. Заполните метаданные (автор, описание)
 *
 * 6. Скомпилируйте и протестируйте:
 *    $ make
 *    $ sudo insmod hello_module.ko
 *    $ dmesg | tail -5
 *    $ sudo rmmod hello_module
 *    $ dmesg | tail -5
 *
 * 7. Протестируйте с параметром:
 *    $ sudo insmod hello_module.ko message="Custom greeting"
 *    $ dmesg | tail -5
 *    $ sudo rmmod hello_module
 *
 * 8. Проверьте параметр в sysfs:
 *    $ cat /sys/module/hello_module/parameters/message
 *
 * ОЖИДАЕМЫЙ РЕЗУЛЬТАТ:
 *
 * $ sudo insmod hello_module.ko
 * $ dmesg | tail -1
 * [12345.678] hello_module: Hello from Ivan Ivanov module!
 *
 * $ sudo insmod hello_module.ko message="Test"
 * $ dmesg | tail -1
 * [12346.789] hello_module: Test
 *
 * $ sudo rmmod hello_module
 * $ dmesg | tail -1
 * [12347.890] hello_module: Goodbye from Ivan Ivanov module!
 */
