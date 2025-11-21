
/*
 * hello_module.c - Задание A: Hello World модуль
 *
 * Простой модуль ядра, который:
 * 1. При загрузке выводит приветствие
 * 2. При выгрузке выводит прощание
 * 3. Принимает параметр message для кастомного сообщения
 */

#include <linux/module.h>      // Для MODULE_*, module_init/exit
#include <linux/kernel.h>      // Для printk, KERN_INFO
#include <linux/init.h>        // Для __init, __exit
#include <linux/moduleparam.h> // Для module_param

// Параметр модуля - кастомное сообщение
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

// Функция инициализации - вызывается при insmod
static int __init hello_init(void)
{
    if (message)
    {
        // Если передан параметр message
        printk(KERN_INFO "Hello Module: %s\n", message);
    }
    else
    {
        // Дефолтное сообщение (ЗАМЕНИТЕ [ВАШ_ИМЯ] на своё имя!)
        printk(KERN_INFO "Hello from Anton Sadovskiy module!\n");
    }

    printk(KERN_INFO "Hello Module: Successfully loaded\n");
    return 0; // 0 = успех
}

// Функция выгрузки - вызывается при rmmod
static void __exit hello_exit(void)
{
    if (message)
    {
        printk(KERN_INFO "Goodbye Module: Custom message was '%s'\n", message);
    }
    else
    {
        printk(KERN_INFO "Goodbye from Anton Sadovskiy module!\n");
    }

    printk(KERN_INFO "Hello Module: Successfully unloaded\n");
}

// Регистрация функций init и exit
module_init(hello_init);
module_exit(hello_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Sadovskiy anton.crykov@gmail.com");
MODULE_DESCRIPTION("Simple Hello World kernel module with parameter support");
MODULE_VERSION("1.0");
