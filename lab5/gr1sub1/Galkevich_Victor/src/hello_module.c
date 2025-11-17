#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#define AUTHOR_NAME "Victor Galkevich"
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR_NAME);
MODULE_DESCRIPTION("Hello World module");

static char *message = NULL;
module_param(message, charp, 0444);
MODULE_PARM_DESC(message, "Custom greeting to print on module load");

static int __init hello_init(void)
{
    const char *default_hello = "Hello from " AUTHOR_NAME " module!";
    printk(KERN_INFO "%s\n", message && *message ? message : default_hello);
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye from " AUTHOR_NAME " module!\n");
}

module_init(hello_init);
module_exit(hello_exit);