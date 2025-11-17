#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static char *kernel_buffer;

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "[mychardev] Device opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "[mychardev] Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    size_t remaining;
    size_t to_copy;

    if (*off >= BUFFER_SIZE) 
        return 0;

    remaining = BUFFER_SIZE - *off;
    to_copy = len < remaining ? len : remaining;

    if (copy_to_user(buf, kernel_buffer + *off, to_copy))
        return -EFAULT;

    *off += to_copy;
    return to_copy;
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
    size_t remaining;
    size_t to_copy;

    if (*off >= BUFFER_SIZE)
        return -ENOSPC;

    remaining = BUFFER_SIZE - *off;
    to_copy = len < remaining ? len : remaining;

    if (copy_from_user(kernel_buffer + *off, buf, to_copy))
        return -EFAULT;

    *off += to_copy;
    return to_copy;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init chardev_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
        return -1;

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }

    memset(kernel_buffer, 0, BUFFER_SIZE);
    printk(KERN_INFO "[mychardev] Device registered with major %d\n", MAJOR(dev_num));
    return 0;
}

static void __exit chardev_exit(void)
{
    kfree(kernel_buffer);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "[mychardev] Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VanyaFilon");
MODULE_DESCRIPTION("Simple character device /dev/mychardev");
