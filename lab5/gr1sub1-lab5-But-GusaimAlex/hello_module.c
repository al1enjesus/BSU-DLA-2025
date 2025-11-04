#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#define MODULE_AUTHOR "Alex_But-Gusaim"
#define MODULE_DESC "Hello world"

static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

static int __init hello_init(void)
{
    if (message)
        printk(KERN_INFO "Hello from %s module!\n", message);
    else
        printk(KERN_INFO "Hello from %s module!\n", MODULE_AUTHOR);

    return 0;
}

static void __exit hello_exit(void)
{
    if (message)
        printk(KERN_INFO "Goodbye from %s module!\n", message);
    else
        printk(KERN_INFO "Goodbye from %s module!\n", MODULE_AUTHOR);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(MODULE_AUTHOR);
MODULE_DESCRIPTION(MODULE_DESC);