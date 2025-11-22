#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static char *message = "Default greeting";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello from Kirill Kurda module!\n");
    printk(KERN_INFO "Message: %s\n", message);
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye from Kirill Kurda module!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Kurda");
MODULE_DESCRIPTION("Lab5 Task A: Hello World Module with Parameters");
MODULE_VERSION("1.0");