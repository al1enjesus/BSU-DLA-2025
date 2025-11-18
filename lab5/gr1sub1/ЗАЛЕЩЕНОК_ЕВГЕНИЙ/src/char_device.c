#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static char *kernel_buffer;
static int buffer_len = 0;

static int dev_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "mychardev: Device opened.\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "mychardev: Device closed.\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    int bytes_to_read;

    if (*ppos >= buffer_len) {
        return 0;
    }

    bytes_to_read = min(count, (size_t)(buffer_len - *ppos));

    if (copy_to_user(ubuf, kernel_buffer + *ppos, bytes_to_read)) {
        return -EFAULT;
    }

    *ppos += bytes_to_read;
    printk(KERN_INFO "mychardev: Read %d bytes.\n", bytes_to_read);
    return bytes_to_read;
}

static ssize_t dev_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    int bytes_to_write;

    if (*ppos >= BUFFER_SIZE) {
        return -ENOSPC;
    }

    bytes_to_write = min(count, (size_t)(BUFFER_SIZE - *ppos));

    if (copy_from_user(kernel_buffer + *ppos, ubuf, bytes_to_write)) {
        return -EFAULT;
    }

    *ppos += bytes_to_write;
    buffer_len = *ppos;
    printk(KERN_INFO "mychardev: Wrote %d bytes.\n", bytes_to_write);
    return bytes_to_write;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init char_dev_init(void) {
    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) return -ENOMEM;

    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        kfree(kernel_buffer);
        return -1;
    }

    my_class = class_create(DEVICE_NAME);

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        class_destroy(my_class);
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return -1;
    }

    device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk(KERN_INFO "mychardev: Module loaded. Major: %d\n", MAJOR(dev_num));
    return 0;
}

static void __exit char_dev_exit(void) {
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(kernel_buffer);
    printk(KERN_INFO "mychardev: Module unloaded.\n");
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhenya");
MODULE_DESCRIPTION("Simple character device driver.");
