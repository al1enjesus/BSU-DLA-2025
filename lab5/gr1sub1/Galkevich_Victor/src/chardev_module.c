#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Galkevich");
MODULE_DESCRIPTION("/dev/mychardev");

#define DEVICE_NAME "mychardev"
#define BUF_SIZE    1024

static dev_t dev_num;
static struct cdev my_cdev;

static char      kbuf[BUF_SIZE];
static size_t    data_size;
static DEFINE_MUTEX(buf_lock);

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device closed\n", DEVICE_NAME);
    return 0;
}

static ssize_t my_read(struct file *file, char __user *ubuf,
                       size_t count, loff_t *ppos)
{
    ssize_t ret = 0;

    if (mutex_lock_interruptible(&buf_lock))
        return -ERESTARTSYS;

    if (*ppos >= data_size)
        goto out;

    {
        size_t to_copy = min(count, data_size - (size_t)*ppos);
        if (copy_to_user(ubuf, kbuf + *ppos, to_copy)) {
            ret = -EFAULT;
            goto out;
        }
        *ppos += to_copy;
        ret = to_copy;
    }

out:
    mutex_unlock(&buf_lock);
    return ret;
}

static ssize_t my_write(struct file *file, const char __user *ubuf,
                        size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (mutex_lock_interruptible(&buf_lock))
        return -ERESTARTSYS;

    {
        size_t to_copy = min(count, (size_t)BUF_SIZE);
        if (copy_from_user(kbuf, ubuf, to_copy)) {
            ret = -EFAULT;
            goto out;
        }
        data_size = to_copy;
        ret = to_copy;
    }

out:
    mutex_unlock(&buf_lock);
    return ret;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
    .llseek  = default_llseek,
};

static int __init my_init(void)
{
    int err;

    err = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (err)
        return err;

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    err = cdev_add(&my_cdev, dev_num, 1);
    if (err) {
        unregister_chrdev_region(dev_num, 1);
        return err;
    }

    data_size = 0;

    pr_info("%s: registered. MAJOR=%u MINOR=%u\n",
            DEVICE_NAME, MAJOR(dev_num), MINOR(dev_num));
    pr_info("Create node manually: mknod /dev/%s c %u 0 && chmod 666 /dev/%s\n",
            DEVICE_NAME, MAJOR(dev_num), DEVICE_NAME);
    return 0;
}

static void __exit my_exit(void)
{
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("%s: unregistered\n", DEVICE_NAME);
}

module_init(my_init);
module_exit(my_exit);