

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define BUF_SIZE 1024



static dev_t dev_num;
static struct cdev my_cdev;
static char device_buffer[BUF_SIZE];
static int buffer_size = 0;


static int dev_open(struct inode *inode, struct file *file)
{


    printk(KERN_INFO "chardev: Device opened (TODO: implement)\n");
    return 0;
}

// TODO: Реализуйте функцию закрытия устройства
// Вызывается при close()
static int dev_release(struct inode *inode, struct file *file)
{
    // TODO: Выведите сообщение в dmesg
    // printk(KERN_INFO "chardev: Device closed\n");

    printk(KERN_INFO "chardev: Device closed (TODO: implement)\n");
    return 0;
}

// TODO: Реализуйте функцию чтения из устройства

static ssize_t dev_read(struct file *file, char __user *buf,
                        size_t len, loff_t *off)
{
    int bytes_to_read;



    printk(KERN_INFO "chardev: Read request (TODO: implement)\n");
    return 0;
}


static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *off)
{
    int bytes_to_write;

    printk(KERN_INFO "chardev: Write request (TODO: implement)\n");
    return len;  // TODO: вернуть реальное количество байт
}

// Таблица операций для устройства
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

// TODO: Функция инициализации модуля
static int __init chardev_init(void)
{
    int ret;

    printk(KERN_INFO "chardev: Initializing\n");

    // TODO: 1. Выделить major и minor номера
    // alloc_chrdev_region выделяет диапазон номеров устройств
    //
    // ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    // if (ret < 0) {
    //     printk(KERN_ERR "chardev: Failed to allocate major number\n");
    //     return ret;
    // }
    //
    // printk(KERN_INFO "chardev: Registered with major number %d\n", MAJOR(dev_num));

    // TODO: 2. Инициализировать cdev структуру
    // cdev_init связывает cdev с file_operations
    //
    // cdev_init(&my_cdev, &fops);
    // my_cdev.owner = THIS_MODULE;

    // TODO: 3. Добавить cdev в систему
    // cdev_add регистрирует устройство в ядре
    //
    // ret = cdev_add(&my_cdev, dev_num, 1);
    // if (ret < 0) {
    //     unregister_chrdev_region(dev_num, 1);
    //     printk(KERN_ERR "chardev: Failed to add cdev\n");
    //     return ret;
    // }

    printk(KERN_INFO "chardev: Device registered (TODO: implement registration)\n");
    printk(KERN_INFO "chardev: Create device with: mknod /dev/%s c <MAJOR> 0\n", DEVICE_NAME);

    return 0;
}

// TODO: Функция выгрузки модуля
static void __exit chardev_exit(void)
{
    // TODO: 1. Удалить cdev из системы
    // cdev_del(&my_cdev);

    // TODO: 2. Освободить major и minor номера
    // unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "chardev: Device unregistered (TODO: implement cleanup)\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");  // TODO: Ваше имя
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");


