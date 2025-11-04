#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanna");
MODULE_DESCRIPTION("Task A: A simple Hello World kernel module with a parameter.");
MODULE_VERSION("1.0");

static char *message = "Hello from Hanna module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "The message to display when the module is loaded.");

static int __init hello_init(void) {
    printk(KERN_INFO "hello_module: %.256s\n", message);
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "hello_module: Goodbye from Hanna module!\n");
}

module_init(hello_init);
module_exit(hello_exit);