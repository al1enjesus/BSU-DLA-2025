/*
 * hello_module.c - Simple Hello World kernel module with parameter
 *
 * Usage:
 *   make
 *   sudo insmod hello_module.ko
 *   sudo insmod hello_module.ko message="Custom greeting"
 *   sudo rmmod hello_module
 *   dmesg | tail -5
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = NULL;
module_param(message, charp, 0444);
MODULE_PARM_DESC(message, "Custom greeting message");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Timofey Trofimovich");
MODULE_DESCRIPTION("Simple Hello World kernel module (Lab 5)");
MODULE_VERSION("1.0");

static int __init hello_init(void)
{
    if (message && message[0] != '\0') {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Timofey Trofimovich module!\n");
    }

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Timofey Trofimovich module!\n");
}

module_init(hello_init);
module_exit(hello_exit);
