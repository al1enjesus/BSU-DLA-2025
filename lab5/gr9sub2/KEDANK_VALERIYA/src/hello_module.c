#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameter");
MODULE_VERSION("1.0");

static char *message = "Hello from Valeriya module!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display on module load");

static int __init hello_init(void)
{
    printk(KERN_INFO "=== Hello Module Loaded ===\n");
    printk(KERN_INFO "Message: %s\n", message);
    printk(KERN_INFO "Module loaded successfully!\n");
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "=== Hello Module Unloaded ===\n");
    printk(KERN_INFO "Goodbye from Valeriya module!\n");
}

module_init(hello_init);
module_exit(hello_exit);