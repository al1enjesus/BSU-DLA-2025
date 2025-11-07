#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static char *message = "Hello from Kaiser Yury,first of his namemodule!";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message parameter");

static int __init hello_init(void) {
    printk(KERN_INFO "=== ЗАГРУЗКА МОДУЛЯ ===\n");
    printk(KERN_INFO "%s\n", message);
    printk(KERN_INFO "Модуль успешно загружен!\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye from Kaiser Yury,first of his name,module!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiser Yury,first of his name");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");

