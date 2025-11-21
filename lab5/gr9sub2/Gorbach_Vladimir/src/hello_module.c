#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gorbach Vladimir");
MODULE_DESCRIPTION("Task A: A simple Hello World kernel module with a parameter.");
MODULE_VERSION("1.0");

static char *message = "Hello from Gorbach Vladimir module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "The message to display when the module is loaded.");

/**
 * hello_init - Функция инициализации модуля
 * Вызывается при загрузке модуля (insmod)
 */
static int __init hello_init(void) {
    /* Ограничиваем вывод 256 символами для безопасности */
    printk(KERN_INFO "hello_module: %.256s\n", message);
    return 0;
}

/**
 * hello_exit - Функция выгрузки модуля
 * Вызывается при выгрузке модуля (rmmod)
 */
static void __exit hello_exit(void) {
    printk(KERN_INFO "hello_module: Goodbye from Gorbach Vladimir module!\n");
}

module_init(hello_init);
module_exit(hello_exit);
