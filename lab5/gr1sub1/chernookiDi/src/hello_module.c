#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chernooki DI");
MODULE_DESCRIPTION("Simple Hello World module");

static char *message = "Hello from Chernooki DI module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Message to display on load");

static int __init hello_init(void)
{
    printk(KERN_INFO "%s\n", message);
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye from Chernooki DI module!\n");
}

module_init(hello_init);
module_exit(hello_exit);