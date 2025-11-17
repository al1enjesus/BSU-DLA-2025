#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

// Параметр модуля "message" (строка)
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

/*
 * Функция инициализации модуля
 * Вызывается при insmod
 */
static int __init hello_init(void)
{
    if (message) {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Ланкуть Александр module!\n");
    }

    return 0;  // 0 = успех
}

/*
 * Функция выгрузки модуля
 * Вызывается при rmmod
 */
static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Ланкуть Александр module!\n");
}

// Регистрация функций init/exit
module_init(hello_init);
module_exit(hello_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ланкуть Александр");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");
