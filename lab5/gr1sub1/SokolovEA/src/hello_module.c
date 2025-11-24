#include <linux/module.h>    // Обязательно для всех модулей
#include <linux/kernel.h>    // Для printk, KERN_*
#include <linux/init.h>      // Для __init, __exit
#include <linux/moduleparam.h>  // Для module_param

// Параметр модуля "message" (строка)
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

// Функция инициализации модуля - вызывается при insmod
static int __init hello_init(void)
{
    if (message) {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Sokolov Egor module!\n");
    }

    return 0;  
}

// Функция выгрузки модуля - вызывается при rmmod
static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Sokolov Egor module!\n");
}

// Регистрация функций init/exit
module_init(hello_init);
module_exit(hello_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sokolov Evgeny");
MODULE_DESCRIPTION("Simple Hello World module for Lab5");
MODULE_VERSION("1.0");