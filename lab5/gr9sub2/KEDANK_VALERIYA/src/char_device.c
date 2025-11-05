#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "mydevice"
#define BUFFER_SIZE 1024

static dev_t dev_num;
static struct cdev my_cdev;
static char *device_buffer;
static int open_count = 0;

static int device_open(struct inode *inode, struct file *file)
{
    open_count++;
    printk(KERN_INFO "char_device: Device opened (%d times)\n", open_count);
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "char_device: Device closed\n");
    return 0;
}

static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t len, loff_t *offset)
{
    int bytes_read = 0;
    char *message = "Hello from Valeriya's character device!\n";
    int message_len = strlen(message);
    
    if (*offset >= message_len)
        return 0;
        
    if (len > message_len - *offset)
        len = message_len - *offset;
    
    if (copy_to_user(user_buf, message + *offset, len)) {
        return -EFAULT;
    }
    
    *offset += len;
    bytes_read = len;
    
    printk(KERN_INFO "char_device: Read %d bytes from device\n", bytes_read);
    return bytes_read;
}

static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t len, loff_t *offset)
{
    int bytes_written;
    
    if (len >= BUFFER_SIZE) {
        printk(KERN_WARNING "char_device: Write too long, truncating to %d bytes\n", 
               BUFFER_SIZE-1);
        len = BUFFER_SIZE - 1;
    }
    
    memset(device_buffer, 0, BUFFER_SIZE);
    
    if (copy_from_user(device_buffer, user_buf, len)) {
        printk(KERN_ERR "char_device: Failed to copy data from user\n");
        return -EFAULT;
    }
    
    bytes_written = len;
    device_buffer[len] = '\0'; // Null terminate
    
    printk(KERN_INFO "char_device: Received %d bytes: %s\n", bytes_written, device_buffer);
    
    return bytes_written;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

static int __init char_device_init(void)
{
    int ret;
    
    // Allocate device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "char_device: Failed to allocate device number\n");
        return ret;
    }
    
    printk(KERN_INFO "char_device: Allocated major number %d, minor %d\n",
           MAJOR(dev_num), MINOR(dev_num));
    
    // Allocate buffer
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        printk(KERN_ERR "char_device: Failed to allocate buffer\n");
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }
    
    // Initialize and add character device
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "char_device: Failed to add character device\n");
        kfree(device_buffer);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    printk(KERN_INFO "char_device: Loaded successfully\n");
    printk(KERN_INFO "char_device: Create device node with: sudo mknod /dev/%s c %d 0\n",
           DEVICE_NAME, MAJOR(dev_num));
    
    return 0;
}

static void __exit char_device_exit(void)
{
    cdev_del(&my_cdev);
    kfree(device_buffer);
    unregister_chrdev_region(dev_num, 1);
    
    printk(KERN_INFO "char_device: Unloaded successfully\n");
    printk(KERN_INFO "char_device: Removed device %s\n", DEVICE_NAME);
}

module_init(char_device_init);
module_exit(char_device_exit);