#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

static int __init hello_init(void)
{
    if (message) {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Pardaev Aleksey module!\n");
    }

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Pardaev Aleksey module!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pardaev Aleksey <pardaev@gmail.com>");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");