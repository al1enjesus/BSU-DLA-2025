#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


#define DRIVER_NAME "simple_char_driver"
#define STORAGE_SIZE 2048 

static dev_t current_dev;
static struct cdev c_dev_struct;
static char internal_storage[STORAGE_SIZE];
static int data_length = 0;


static int driver_open_handler(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "simple_driver: Port opened by user\n");
    return 0;
}

static int driver_close_handler(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "simple_driver: Port closed\n");
    return 0;
}

static ssize_t driver_read_handler(struct file *file, char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
    int bytes_read;
    int max_bytes;

    
    if (*ppos >= data_length) {
        return 0;
    }


    max_bytes = data_length - *ppos;
    bytes_read = (max_bytes > count) ? count : max_bytes;

    if (copy_to_user(user_buf, internal_storage + *ppos, bytes_read)) {
        printk(KERN_ERR "simple_driver: Copy to user failed\n");
        return -EFAULT;
    }

    *ppos += bytes_read;
    printk(KERN_INFO "simple_driver: Sent %d bytes to user space\n", bytes_read);

    return bytes_read;
}

static ssize_t driver_write_handler(struct file *file, const char __user *user_buf,
                                    size_t count, loff_t *ppos)
{
    int capacity = STORAGE_SIZE;
    int bytes_to_write;

    
    if (count > capacity) {
        bytes_to_write = capacity;
    } else {
        bytes_to_write = count;
    }

    
    memset(internal_storage, 0, STORAGE_SIZE);

    if (copy_from_user(internal_storage, user_buf, bytes_to_write)) {
        printk(KERN_ERR "simple_driver: Copy from user failed\n");
        return -EFAULT;
    }

    data_length = bytes_to_write;
    printk(KERN_INFO "simple_driver: Received %d bytes\n", bytes_to_write);

    return bytes_to_write;
}


static struct file_operations fops_struct = {
    .owner = THIS_MODULE,
    .open = driver_open_handler,
    .release = driver_close_handler,
    .read = driver_read_handler,
    .write = driver_write_handler,
};

static int __init my_driver_init(void)
{
    int error_code;

    printk(KERN_INFO "simple_driver: Starting initialization...\n");

   
    error_code = alloc_chrdev_region(&current_dev, 0, 1, DRIVER_NAME);
    if (error_code < 0) {
        printk(KERN_ALERT "simple_driver: Major number allocation failed\n");
        return error_code;
    }

    printk(KERN_INFO "simple_driver: Major number allocated: %d\n", MAJOR(current_dev));

    cdev_init(&c_dev_struct, &fops_struct);
    
    c_dev_struct.owner = THIS_MODULE;

    error_code = cdev_add(&c_dev_struct, current_dev, 1);
    if (error_code < 0) {
        unregister_chrdev_region(current_dev, 1);
        printk(KERN_ERR "simple_driver: Cdev registration failed\n");
        return error_code;
    }

    printk(KERN_INFO "simple_driver: Driver ready.\n");
    printk(KERN_INFO "Command to create node: sudo mknod /dev/%s c %d 0\n",
           DRIVER_NAME, MAJOR(current_dev));

    return 0;
}

static void __exit my_driver_exit(void)
{
    cdev_del(&c_dev_struct);
    unregister_chrdev_region(current_dev, 1);
    printk(KERN_INFO "simple_driver: Cleanup complete\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("[Vanli Ruslan]");
MODULE_DESCRIPTION("A rewritten character device driver");
MODULE_VERSION("2.0");
