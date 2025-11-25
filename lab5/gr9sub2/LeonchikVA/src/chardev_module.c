#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "mychardev"
#define BUF_SIZE 1024

// Глобальные переменные
static dev_t dev_num;              // Major и minor номера устройства
static struct cdev my_cdev;        // Character device структура
static struct class *dev_class;    // Класс устройства
static char *kernel_buffer;        // Буфер для хранения данных
static size_t buffer_size = 0;     // Текущий размер данных в буфере
static int device_open_count = 0;  // Счётчик открытий устройства

// Функция открытия устройства
static int dev_open(struct inode *inode, struct file *file) {
    device_open_count++;
    printk(KERN_INFO "chardev_module: Device opened (open count: %d)\n", 
           device_open_count);
    return 0;
}

// Функция закрытия устройства
static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "chardev_module: Device closed\n");
    return 0;
}

// Функция чтения из устройства
static ssize_t dev_read(struct file *file, char __user *user_buf,
                        size_t count, loff_t *ppos) {
    size_t to_read;

    // Если позиция за пределами данных, возвращаем EOF
    if (*ppos >= buffer_size)
        return 0;

    // Определяем, сколько байт можно прочитать
    to_read = buffer_size - *ppos;
    if (to_read > count)
        to_read = count;

    // Копируем данные из kernel space в user space
    if (copy_to_user(user_buf, kernel_buffer + *ppos, to_read)) {
        printk(KERN_ERR "chardev_module: Failed to copy data to user\n");
        return -EFAULT;
    }

    *ppos += to_read;

    printk(KERN_INFO "chardev_module: Read %zu bytes from device\n", to_read);

    return to_read;
}

// Функция записи в устройство
static ssize_t dev_write(struct file *file, const char __user *user_buf,
                         size_t count, loff_t *ppos) {
    size_t to_write;

    // Ограничиваем запись размером буфера
    to_write = count;
    if (to_write > BUF_SIZE)
        to_write = BUF_SIZE;

    // Копируем данные из user space в kernel space
    if (copy_from_user(kernel_buffer, user_buf, to_write)) {
        printk(KERN_ERR "chardev_module: Failed to copy data from user\n");
        return -EFAULT;
    }

    buffer_size = to_write;

    printk(KERN_INFO "chardev_module: Wrote %zu bytes to device\n", to_write);

    return to_write;
}

// Структура файловых операций
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

// Функция инициализации модуля
static int __init chardev_init(void) {
    int ret;

    // Выделяем память для буфера
    kernel_buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "chardev_module: Failed to allocate memory\n");
        return -ENOMEM;
    }
    memset(kernel_buffer, 0, BUF_SIZE);

    // Динамически выделяем major и minor номера
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev_module: Failed to allocate device numbers\n");
        kfree(kernel_buffer);
        return ret;
    }

    printk(KERN_INFO "chardev_module: Registered device with major=%d, minor=%d\n",
           MAJOR(dev_num), MINOR(dev_num));

    // Инициализируем character device
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    // Добавляем character device в систему
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "chardev_module: Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return ret;
    }

    // Создаём класс устройства
    dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "chardev_module: Failed to create device class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return PTR_ERR(dev_class);
    }

    // Создаём файл устройства /dev/mychardev
    if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        printk(KERN_ERR "chardev_module: Failed to create device\n");
        class_destroy(dev_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return -1;
    }

    printk(KERN_INFO "chardev_module: Device /dev/%s created successfully\n", 
           DEVICE_NAME);
    printk(KERN_INFO "chardev_module: Use 'sudo mknod /dev/%s c %d 0' if device not auto-created\n",
           DEVICE_NAME, MAJOR(dev_num));

    return 0;
}

// Функция выгрузки модуля
static void __exit chardev_exit(void) {
    // Удаляем файл устройства
    device_destroy(dev_class, dev_num);

    // Удаляем класс устройства
    class_destroy(dev_class);

    // Удаляем character device
    cdev_del(&my_cdev);

    // Освобождаем major и minor номера
    unregister_chrdev_region(dev_num, 1);

    // Освобождаем буфер
    kfree(kernel_buffer);

    printk(KERN_INFO "chardev_module: Device unregistered, total opens: %d\n",
           device_open_count);
}

// Регистрация функций init/exit
module_init(chardev_init);
module_exit(chardev_exit);

// Метаданные модуля
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonchik Vladislav");
MODULE_DESCRIPTION("Simple character device driver with read/write support");
MODULE_VERSION("1.0");