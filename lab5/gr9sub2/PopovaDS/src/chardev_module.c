#include <linux/module.h>
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


cdev_init(&my_cdev, &fops);
my_cdev.owner = THIS_MODULE;


ret = cdev_add(&my_cdev, dev_num, 1);
if (ret) {
unregister_chrdev_region(dev_num, 1);
printk(KERN_ERR "chardev: Failed to add cdev\n");
return ret;
}


printk(KERN_INFO "chardev: Registered with major number %d\n", MAJOR(dev_num));
printk(KERN_INFO "chardev: Create device with: sudo mknod /dev/%s c %d 0 && sudo chmod 666 /dev/%s\n",
DEVICE_NAME, MAJOR(dev_num), DEVICE_NAME);
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
MODULE_AUTHOR("Dasha <student@example.com>");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");
