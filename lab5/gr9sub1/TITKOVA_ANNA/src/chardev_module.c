/*
 * chardev_module.c - Простой character device
 *
 * Создаёт устройство /dev/mychardev, которое:
 * - Принимает данные при записи (до 1024 байт)
 * - Возвращает сохранённые данные при чтении
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mychardev"
#define BUF_SIZE 1024

// Глобальные переменные
static dev_t dev_num;
static struct cdev my_cdev;
static char device_buffer[BUF_SIZE];
static int buffer_size = 0;

// Функция открытия устройства
static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: Device opened\n");
    return 0;
}

// Функция закрытия устройства
static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: Device closed\n");
    return 0;
}

// Функция чтения из устройства
static ssize_t dev_read(struct file *file, char __user *buf,
                        size_t len, loff_t *off)
{
    int bytes_to_read;

    // Проверяем, есть ли данные для чтения
    if (*off >= buffer_size)
        return 0;

    // Вычисляем, сколько байт можно прочитать
    bytes_to_read = min(len, (size_t)(buffer_size - *off));

    // Скопируйте данные из device_buffer в user space
    if (copy_to_user(buf, device_buffer + *off, bytes_to_read))
        return -EFAULT;

    // Обновляем позицию чтения
    *off += bytes_to_read;

    return bytes_to_read;
}

// Функция записи в устройство
static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *off)
{
    int bytes_to_write;

    // Вычисляем, сколько байт можно записать (не больше BUF_SIZE)
    bytes_to_write = min(len, (size_t)BUF_SIZE);

    // Очищаем буфер перед записью
    memset(device_buffer, 0, BUF_SIZE);

    // Скопируйте данные из user space в device_buffer
    if (copy_from_user(device_buffer, buf, bytes_to_write))
        return -EFAULT;

    // Сохраняем размер данных
    buffer_size = bytes_to_write;

    // Обнуляем позицию чтения
    *off = 0;

    printk(KERN_INFO "chardev: Wrote %d bytes\n", bytes_to_write);

    return bytes_to_write;
}

// Таблица операций для устройства
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

// Функция инициализации модуля
static int __init chardev_init(void)
{
    int ret;

    printk(KERN_INFO "chardev: Initializing\n");

    // Выделяем major и minor номера
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to allocate major number\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Registered with major number %d\n", MAJOR(dev_num));

    // Инициализируем cdev структуру
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    // Добавляем cdev в систему
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "chardev: Failed to add cdev\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Device registered successfully\n");
    printk(KERN_INFO "chardev: Create device with: sudo mknod /dev/%s c %d 0\n", 
           DEVICE_NAME, MAJOR(dev_num));

    return 0;
}

// Функция выгрузки модуля
static void __exit chardev_exit(void)
{
    // Удаляем cdev из системы
    cdev_del(&my_cdev);

    // Освобождаем major и minor номера
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "chardev: Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Titkova Anna");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");
