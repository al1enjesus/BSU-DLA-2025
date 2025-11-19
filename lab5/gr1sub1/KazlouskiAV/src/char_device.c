#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kazlouski Anton");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static char *device_buffer;
static int buffer_used = 0;

static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Character device opened by process\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Character device closed\n");
    return 0;
}

static ssize_t device_read(struct file *file, char __user *user_buf, size_t count, loff_t *offset) {
ssize_t bytes_read = 0;

if (*offset >= buffer_used) {
return 0;
}

if (*offset + count > buffer_used) {
count = buffer_used - *offset;
}

if (copy_to_user(user_buf, device_buffer + *offset, count)) {
return -EFAULT;
}

*offset += count;
bytes_read = count;

printk(KERN_INFO "Read %zd bytes from device\n", bytes_read);
return bytes_read;
}

static ssize_t device_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offset) {
ssize_t bytes_written = 0;

if (count > BUFFER_SIZE) {
count = BUFFER_SIZE;
}

if (copy_from_user(device_buffer, user_buf, count)) {
return -EFAULT;
}

buffer_used = count;
bytes_written = count;

printk(KERN_INFO "Wrote %zd bytes to device: %.*s\n", bytes_written, (int)count, device_buffer);
return bytes_written;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

static int __init char_init(void) {
    int ret;

    // Allocate major number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }

    // Allocate buffer
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to allocate buffer\n");
        return -ENOMEM;
    }

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to add character device\n");
        return ret;
    }

    printk(KERN_INFO "Character device registered with major: %d\n", MAJOR(dev_num));
    printk(KERN_INFO "Create device node with: sudo mknod /dev/%s c %d 0\n",
        DEVICE_NAME, MAJOR(dev_num));

    return 0;
}

static void __exit char_exit(void) {
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(device_buffer);

    printk(KERN_INFO "Character device unregistered\n");
}

module_init(char_init);
module_exit(char_exit);