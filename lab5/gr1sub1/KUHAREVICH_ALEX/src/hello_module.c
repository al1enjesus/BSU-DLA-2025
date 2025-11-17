#include <linux/module.h>   
#include <linux/kernel.h>   
#include <linux/init.h>        
#include <linux/moduleparam.h> 

#define MY_NAME "Kuharevich Alexander"

static char *message = "Default greeting"; 
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to be printed upon module load");

MODULE_LICENSE("GPL");
MODULE_AUTHOR(MY_NAME);
MODULE_DESCRIPTION("Lab 5: Simple Hello World Module with parameter.");
MODULE_VERSION("1.0");

static int __init hello_init(void) {
    printk(KERN_INFO "--- Hello Module Init ---\n");
    printk(KERN_INFO "Hello from %s module! Message: %s\n", MY_NAME, message);
    printk(KERN_INFO "-------------------------\n");
    return 0; 
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "--- Hello Module Exit ---\n");
    printk(KERN_INFO "Goodbye from %s module!\n", MY_NAME);
    printk(KERN_INFO "-------------------------\n");
}

module_init(hello_init);
module_exit(hello_exit);

