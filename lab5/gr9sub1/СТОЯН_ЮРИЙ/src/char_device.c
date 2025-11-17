#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define BUFFER_SIZE 1024
#define CLASS_NAME "chardev_class"

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *dev_class;
static char device_buffer[BUFFER_SIZE];
static int buffer_used = 0;

// Функция открытия устройства
static int dev_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Character device opened\n");
    return 0;
}

// Функция закрытия устройства
static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Character device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    if (*off >= buffer_used || buffer_used == 0) {
        return 0;
    }
  
    if (len > buffer_used - *off) {
        len = buffer_used - *off;
    }
    
    // Копируем данные из ядра в пользовательское пространство
    if (copy_to_user(buf, device_buffer + *off, len)) {
        return -EFAULT;
    }
    
    *off += len;
    printk(KERN_INFO "Read %zu bytes from device\n", len);
    return len;
}

// Функция записи в устройство
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    // Проверяем размер данных
    if (len > BUFFER_SIZE) {
        printk(KERN_WARNING "Write too large: %zu bytes\n", len);
        return -ENOMEM;
    }
    
    // Копируем данные из пользовательского пространства в ядро
    if (copy_from_user(device_buffer, buf, len)) {
        return -EFAULT;
    }
    
    buffer_used = len;
    *off = len;
    
    printk(KERN_INFO "Written %zu bytes to device\n", len);
    printk(KERN_INFO "Data: %.*s\n", (int)len, device_buffer);
    
    return len;
}

// Структура файловых операций
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init dev_init(void) {
    int ret;
    
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device numbers\n");
        return ret;
    }
    
    printk(KERN_INFO "Character device registered with major: %d, minor: %d\n", 
           MAJOR(dev_num), MINOR(dev_num));
    
    // Инициализируем cdev
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    
    // Добавляем character device в систему
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    // Создаём класс устройства
    dev_class = class_create(CLASS_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "Failed to create device class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(dev_class);
    }
    
    // Создаём device node
    device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME);
    
    printk(KERN_INFO "Character device %s initialized successfully\n", DEVICE_NAME);
    return 0;
}

static void __exit dev_exit(void) {
    // Удаляем device node
    device_destroy(dev_class, dev_num);
    
    // Удаляем класс
    class_destroy(dev_class);
    
    // Удаляем cdev
    cdev_del(&my_cdev);
    
    // Освобождаем номера устройства
    unregister_chrdev_region(dev_num, 1);
    
    printk(KERN_INFO "Character device %s unregistered\n", DEVICE_NAME);
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiser Yury");
MODULE_DESCRIPTION("Simple Character Device");
