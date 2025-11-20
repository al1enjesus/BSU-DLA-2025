#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "mychardev"
#define BUF_SIZE 1024

static char device_buffer[BUF_SIZE];
static size_t buffer_size = 0;

static struct cdev my_cdev;
static dev_t dev_num;

static DEFINE_MUTEX(chardev_mutex);

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
                        size_t count, loff_t *ppos)
{
    ssize_t bytes;

    if (mutex_lock_interruptible(&chardev_mutex))
        return -ERESTARTSYS;

    if (*ppos >= buffer_size) {
        mutex_unlock(&chardev_mutex);
        return 0;
    }

    bytes = min(count, buffer_size - *ppos);

    if (copy_to_user(buf, device_buffer + *ppos, bytes)) {
        mutex_unlock(&chardev_mutex);
        return -EFAULT;
    }

    *ppos += bytes;
    mutex_unlock(&chardev_mutex);

    printk(KERN_INFO "chardev: Read %zd bytes\n", bytes);
    return bytes;
}

static ssize_t dev_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    ssize_t bytes_to_write = min(count, (size_t)(BUF_SIZE - 1));

    if (mutex_lock_interruptible(&chardev_mutex))
        return -ERESTARTSYS;

    memset(device_buffer, 0, BUF_SIZE);

    if (copy_from_user(device_buffer, buf, bytes_to_write)) {
        mutex_unlock(&chardev_mutex);
        return -EFAULT;
    }

    buffer_size = bytes_to_write;
    device_buffer[buffer_size] = '\0';

    mutex_unlock(&chardev_mutex);

    printk(KERN_INFO "chardev: Write %zd bytes\n", bytes_to_write);
    return bytes_to_write;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
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

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "chardev: Failed to add cdev\n");
        return ret;
    }

    printk(KERN_INFO "chardev: Registered with major %d\n", MAJOR(dev_num));
    printk(KERN_INFO "To create device node:\n");
    printk(KERN_INFO "sudo mknod /dev/%s c %d 0\n", DEVICE_NAME, MAJOR(dev_num));
    printk(KERN_INFO "sudo chmod 666 /dev/%s\n", DEVICE_NAME);

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
MODULE_AUTHOR("Dasha");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

