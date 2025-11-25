#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// Параметр модуля для кастомного сообщения
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

// Функция инициализации (вызывается при insmod)
static int __init hello_init(void) {
    if (message) {
        // Если параметр задан, выводим его
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        // Иначе выводим дефолтное сообщение
        printk(KERN_INFO "hello_module: Hello from Leonchik Vladislav module!\n");
    }
    return 0;  // 0 = успешная загрузка
}

// Функция выгрузки (вызывается при rmmod)
static void __exit hello_exit(void) {
    if (message) {
        printk(KERN_INFO "hello_module: Goodbye (custom message was: %s)\n", message);
    } else {
        printk(KERN_INFO "hello_module: Goodbye from Leonchik Vladislav module!\n");
    }
}

// Регистрация функций init/exit
module_init(hello_init);
module_exit(hello_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonchik Vladislav");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameter support");
MODULE_VERSION("1.0");