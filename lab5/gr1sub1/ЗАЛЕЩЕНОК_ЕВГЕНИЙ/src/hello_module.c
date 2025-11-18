#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>

static char *message = "Hello from zhenya module!";
module_param(message, charp, 0444);
MODULE_PARM_DESC(message, "A custom message to display upon loading");

static int __init hello_init(void) {
    printk(KERN_INFO "%s\n", message);
    return 0; 
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye from zhenya module!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ваше Имя");
MODULE_DESCRIPTION("A simple kernel module with a parameter.");
MODULE_VERSION("1.0");
