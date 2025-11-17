#include <linux/init.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static char* init_output_string = "Hello from Nikita module!";
module_param(init_output_string, charp, 0000);
MODULE_PARM_DESC(init_output_string, "A character init output string");

static char* exit_output_string = "Goodbye from Nikita module!";
module_param(exit_output_string, charp, 0000);
MODULE_PARM_DESC(exit_output_string, "A character exit output string");

static int __init hello_world_module_init(void)
{  
    pr_info("%s\n", init_output_string);  

    return 0;  
}  

static void __exit hello_world_module_exit(void)  
{
    pr_info("%s\n", exit_output_string);  
}  

module_init(hello_world_module_init);  
module_exit(hello_world_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikita Lukyanov");
MODULE_DESCRIPTION("Hello world module, variant 2");