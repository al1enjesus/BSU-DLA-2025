#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = "Hello from Alesia module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

static int __init hello_init(void) {
    printk(KERN_INFO "%s\n", message);
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye from Alesia module!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alesia");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");
