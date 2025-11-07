#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameters");
MODULE_VERSION("1.0");

// Параметр модуля: строка message
static char *message = "default message";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

// Функция инициализации модуля
static int __init hello_init(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Hello from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Message from module: %s\n", message);
    }
    return 0;
}

// Функция выгрузки модуля
static void __exit hello_exit(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Goodbye from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Goodbye! Last message was: %s\n", message);
    }
}

// Регистрация функций
module_init(hello_init);
module_exit(hello_exit);
