#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = "Hello from Vanya's module!";
static char *byeMessage = "Goodbye from Vanya's module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message");

static int __init hello_init(void)
{
printk(KERN_INFO "[hello_module] %s\n", message);
return 0;
}

static void __exit hello_exit(void)
{
printk(KERN_INFO "[hello_module] %s\n", byeMessage);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Hello World Kernel Module");