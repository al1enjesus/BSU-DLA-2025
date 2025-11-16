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
static char device_buffer[BUF_SIZE];
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
        return 0;  // EOF

    bytes_to_read = buffer_size - *off;
    if (bytes_to_read > len)
        bytes_to_read = len;

    if (copy_to_user(buf, device_buffer + *off, bytes_to_read)) {
        printk(KERN_ERR "chardev: Failed to copy data to user\n");
        return -EFAULT;
    }

    *off += bytes_to_read;

    printk(KERN_INFO "chardev: Read %d bytes\n", bytes_to_read);

    return bytes_to_read;
}

static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *off)
{
    int bytes_to_write;

    bytes_to_write = len;
    if (bytes_to_write > BUF_SIZE)
        bytes_to_write = BUF_SIZE;

    memset(device_buffer, 0, BUF_SIZE);

    if (copy_from_user(device_buffer, buf, bytes_to_write)) {
        printk(KERN_ERR "chardev: Failed to copy data from user\n");
        return -EFAULT;
    }

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

static int __init chardev_init(void)
{
    int ret;

    printk(KERN_INFO "chardev: Initializing\n");

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to allocate major number\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Registered with major number %d\n", MAJOR(dev_num));

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

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

static void __exit chardev_exit(void)
{
    cdev_del(&my_cdev);

    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "chardev: Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lutski Vadzim <vadim.lutski.a@gmail.com>");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");