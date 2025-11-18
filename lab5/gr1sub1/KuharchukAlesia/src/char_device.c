#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024
#define CLASS_NAME "alesia_class"

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *dev_class;
static char *device_buffer;
static int open_count = 0;

static int dev_open(struct inode *inode, struct file *file) {
    open_count++;
    printk(KERN_INFO "mychardev: Device opened (%d times)\n", open_count);
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "mychardev: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, 
                       size_t len, loff_t *offset) {
    ssize_t bytes_read = 0;
    size_t buffer_len = strlen(device_buffer);
    
    if (*offset >= buffer_len) {
        return 0;
    }
    
    if (len > buffer_len - *offset) {
        len = buffer_len - *offset;
    }
    
    if (copy_to_user(buf, device_buffer + *offset, len)) {
        return -EFAULT;
    }
    
    *offset += len;
    bytes_read = len;
    
    printk(KERN_INFO "mychardev: Read %zd bytes\n", bytes_read);
    return bytes_read;
}

static ssize_t dev_write(struct file *file, const char __user *buf, 
                        size_t len, loff_t *offset) {
    ssize_t bytes_written = 0;
    
    if (len > BUFFER_SIZE - 1) {
        len = BUFFER_SIZE - 1;
    }
    
    if (copy_from_user(device_buffer, buf, len)) {
        return -EFAULT;
    }
    
    device_buffer[len] = '\0';
    bytes_written = len;
    
    printk(KERN_INFO "mychardev: Wrote %zd bytes: %s\n", bytes_written, device_buffer);
    return bytes_written;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init chardev_init(void) {
    int ret;
    
    // Выделяем major/minor номера
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device numbers\n");
        return ret;
    }
    
    // Выделяем буфер
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }
    strcpy(device_buffer, "Initial message from Alesia's device");
    
    // Инициализируем cdev
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    
    // Добавляем cdev в систему
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to add cdev\n");
        return ret;
    }
    
    // Создаем класс устройства (автоматически создает /dev/mychardev)
    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dev_class)) {
        cdev_del(&my_cdev);
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to create device class\n");
        return PTR_ERR(dev_class);
    }
    
    // Создаем device node
    device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME);
    
    printk(KERN_INFO "mychardev: Registered with major %d, minor %d\n",
           MAJOR(dev_num), MINOR(dev_num));
    return 0;
}

static void __exit chardev_exit(void) {
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&my_cdev);
    kfree(device_buffer);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "mychardev: Unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alesia");
MODULE_DESCRIPTION("Simple character device driver for Variant 1");
