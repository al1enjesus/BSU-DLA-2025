#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *input_name = "Ruslan";

module_param(input_name, charp, 0644);
MODULE_PARM_DESC(input_name, "Name to display in the log");

static int __init start_hello_module(void)
{
   
    printk(KERN_INFO "custom_hello: Module loaded successfully.\n");
    
    printk(KERN_INFO "custom_hello: Greetings, %s!\n", input_name);

    return 0;
}

static void __exit stop_hello_module(void)
{
    
    printk(KERN_INFO "custom_hello: Module is unloading. See you later, %s!\n", input_name);
}


module_init(start_hello_module);
module_exit(stop_hello_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("[Vanli Ruslan]");
MODULE_DESCRIPTION("Refactored Hello World Kernel Module");
MODULE_VERSION("2.0");
