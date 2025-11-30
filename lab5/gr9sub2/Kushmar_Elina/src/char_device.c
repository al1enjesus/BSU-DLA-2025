/*
 * char_device.c - Простой character device
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

static dev_t dev_num;
static struct cdev my_cdev;
static char *device_buffer;
static int buffer_size = 0;

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: Device opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf,
                        size_t len, loff_t *off)
{
    int bytes_to_read;

    if (*off >= buffer_size)
        return 0;

    bytes_to_read = min_t(size_t, len, buffer_size - *off);

    if (copy_to_user(buf, device_buffer + *off, bytes_to_read))
        return -EFAULT;

    *off += bytes_to_read;
    return bytes_to_read;
}

static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *off)
{
    int bytes_to_write;

    bytes_to_write = min_t(size_t, len, BUF_SIZE);

    memset(device_buffer, 0, BUF_SIZE);
    
    if (copy_from_user(device_buffer, buf, bytes_to_write))
        return -EFAULT;

    buffer_size = bytes_to_write;
    printk(KERN_INFO "chardev: Written %d bytes\n", bytes_to_write);

    return bytes_to_write;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init char_device_init(void)
{
    int ret;

    printk(KERN_INFO "chardev: Initializing\n");

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to allocate major number\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Registered with major number %d\n", MAJOR(dev_num));

    device_buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "chardev: Failed to allocate buffer\n");
        return -ENOMEM;
    }

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "chardev: Failed to add cdev\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Device registered successfully\n");
    printk(KERN_INFO "chardev: Create device with: mknod /dev/%s c %d 0\n", 
           DEVICE_NAME, MAJOR(dev_num));

    return 0;
}

static void __exit char_device_exit(void)
{
    cdev_del(&my_cdev);
    kfree(device_buffer);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "chardev: Device unregistered\n");
}

module_init(char_device_init);
module_exit(char_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Элина Кушмар");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");