#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kazlouski Anton");
MODULE_DESCRIPTION("Simple Hello World module with parameters");
MODULE_VERSION("1.0");

static char *message = "default message";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

static int __init hello_init(void) {
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Hello from Kazlouski Anton module!\n");
    } else {
        printk(KERN_INFO "Custom message: %s\n", message);
    }
    printk(KERN_INFO "Hello module loaded successfully\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye from Kazlouski Anton module!\n");
    printk(KERN_INFO "Hello module unloaded successfully\n");
}

module_init(hello_init);
module_exit(hello_exit);