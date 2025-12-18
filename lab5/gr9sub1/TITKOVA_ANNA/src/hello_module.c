/*
 * hello_module.c - Простейший модуль ядра "Hello World"
 *
 * Компиляция: make
 * Загрузка: sudo insmod hello_module.ko
 * Выгрузка: sudo rmmod hello_module
 * Логи: dmesg | tail
 */

#include <linux/module.h>    // Обязательно для всех модулей
#include <linux/kernel.h>    // Для printk, KERN_*
#include <linux/init.h>      // Для __init, __exit
#include <linux/moduleparam.h>  // Для module_param

// Параметр модуля "message" (строка)
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

// Функция инициализации модуля
static int __init hello_init(void)
{
    if (message) {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Titkova Anna module!\n");
    }

    return 0;  // 0 = успех
}

// Функция выгрузки модуля
static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Titkova Anna module!\n");
}

// Регистрация функций init/exit
module_init(hello_init);
module_exit(hello_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Titkova Anna <anna.titkova@example.com>");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");
