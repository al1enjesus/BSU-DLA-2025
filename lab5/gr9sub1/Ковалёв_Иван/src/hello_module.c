#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameters");
MODULE_VERSION("1.0");

// Максимальная длина сообщения
#define MAX_MESSAGE_LEN 128

// Параметр модуля: строка message
static char message[MAX_MESSAGE_LEN] = "default message";

// Функция для валидации параметра
static int message_param_set(const char *val, const struct kernel_param *kp)
{
    size_t len;
    
    if (!val)
        return -EINVAL;
    
    len = strlen(val);
    if (len >= MAX_MESSAGE_LEN) {
        printk(KERN_WARNING "hello_module: Message too long (%zu), truncating to %d\n", 
               len, MAX_MESSAGE_LEN - 1);
        strncpy(message, val, MAX_MESSAGE_LEN - 1);
        message[MAX_MESSAGE_LEN - 1] = '\0';
        return 0;
    }
    
    strcpy(message, val);
    return 0;
}

static const struct kernel_param_ops message_param_ops = {
    .set = message_param_set,
    .get = param_get_string,
};

module_param_cb(message, &message_param_ops, &message, 0644);
MODULE_PARM_DESC(message, "Custom message to display (max 127 chars)");

// Функция инициализации модуля
static int __init hello_init(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Hello from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Message from module: %s\n", message);
    }
    return 0;
}

// Функция выгрузки модуля
static void __exit hello_exit(void)
{
    if (strcmp(message, "default message") == 0) {
        printk(KERN_INFO "Goodbye from Ковалёв Иван module!\n");
    } else {
        printk(KERN_INFO "Goodbye! Last message was: %s\n", message);
    }
}

// Регистрация функций
module_init(hello_init);
module_exit(hello_exit);
