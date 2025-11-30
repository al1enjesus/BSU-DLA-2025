#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Дехтярёв Арсений");
MODULE_DESCRIPTION("Character device module with mmap and ioctl");
MODULE_VERSION("1.1");

#define DEVICE_NAME "dehtyarev_device"
#define IOCTL_CLEAR_BUFFER _IO('d', 1)

#define BUFFER_SIZE 4096

static dev_t dev_number;
static struct cdev cdev_struct;
static char *device_buffer;   // теперь динамический буфер

//------------------------------------------------------------
// OPEN / RELEASE
//------------------------------------------------------------

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s: device opened\n", DEVICE_NAME);
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s: device closed\n", DEVICE_NAME);
    return 0;
}

//------------------------------------------------------------
// READ / WRITE
//------------------------------------------------------------

static ssize_t dev_read(struct file *file,
                        char __user *user_buffer,
                        size_t size,
                        loff_t *offset)
{
    size_t available = BUFFER_SIZE - *offset;
    size_t to_read = min(size, available);

    if (to_read == 0)
        return 0;

    if (copy_to_user(user_buffer, device_buffer + *offset, to_read))
        return -EFAULT;

    *offset += to_read;
    return to_read;
}

static ssize_t dev_write(struct file *file,
                         const char __user *user_buffer,
                         size_t size,
                         loff_t *offset)
{
    size_t available = BUFFER_SIZE - *offset;
    size_t to_write = min(size, available);

    if (to_write == 0)
        return -ENOSPC;  // Исправлено: -ENOMEM -> -ENOSPC

    if (copy_from_user(device_buffer + *offset, user_buffer, to_write))
        return -EFAULT;

    *offset += to_write;
    return to_write;
}

//------------------------------------------------------------
// IOCTL
//------------------------------------------------------------

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case IOCTL_CLEAR_BUFFER:
            memset(device_buffer, 0, BUFFER_SIZE);
            printk(KERN_INFO "%s: buffer cleared via ioctl\n", DEVICE_NAME);
            return 0;

        default:
            return -EINVAL;
    }
}

//------------------------------------------------------------
// MMAP — корректная реализация для ядра 6.x
//------------------------------------------------------------

static int dev_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size > BUFFER_SIZE)
        return -EINVAL;

    return remap_pfn_range(
        vma,
        vma->vm_start,
        page_to_pfn(virt_to_page(device_buffer)),
        size,
        vma->vm_page_prot
    );
}

//------------------------------------------------------------
// FILE OPERATIONS
//------------------------------------------------------------

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = dev_open,
    .release        = dev_release,
    .read           = dev_read,
    .write          = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .mmap           = dev_mmap,
};

//------------------------------------------------------------
// INIT / EXIT
//------------------------------------------------------------

static int __init char_device_init(void)
{
    int result;

    // allocate device number
    result = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (result < 0) {
        printk(KERN_ERR "Failed to allocate chrdev region\n");
        return result;
    }

    // create cdev
    cdev_init(&cdev_struct, &fops);
    cdev_struct.owner = THIS_MODULE;

    result = cdev_add(&cdev_struct, dev_number, 1);
    if (result < 0) {
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to add cdev\n");
        return result;
    }

    // allocate buffer (physically contiguous memory)
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        cdev_del(&cdev_struct);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to allocate device buffer\n");
        return -ENOMEM;
    }

    memset(device_buffer, 0, BUFFER_SIZE);

    printk(KERN_INFO "%s registered with major %d minor %d\n",
           DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));

    return 0;
}

static void __exit char_device_exit(void)
{
    kfree(device_buffer);
    cdev_del(&cdev_struct);
    unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "%s unregistered\n", DEVICE_NAME);
}

module_init(char_device_init);
module_exit(char_device_exit);