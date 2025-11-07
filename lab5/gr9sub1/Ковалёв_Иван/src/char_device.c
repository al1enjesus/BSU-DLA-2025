#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ковалёв Иван");
MODULE_DESCRIPTION("Simple character device driver");
MODULE_VERSION("1.0");

// Максимальный размер буфера
#define DEVICE_BUFFER_SIZE 1024
#define DEVICE_NAME "mychardev"

// Структура для хранения состояния устройства
struct chardev_data {
    struct cdev cdev;
    char buffer[DEVICE_BUFFER_SIZE];
    size_t buffer_size;
    atomic_t open_count;
    struct mutex buffer_mutex;
};

static int major_number = 0;
static struct chardev_data *device_data = NULL;
static struct class *char_class = NULL;
static struct device *char_device = NULL;  // Добавляем указатель на устройство

// Прототипы функций file_operations
static int device_open(struct inode *inode, struct file *file);
static int device_release(struct inode *inode, struct file *file);
static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t count, loff_t *offset);
static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t count, loff_t *offset);

// Структура file_operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

// Функция открытия устройства
static int device_open(struct inode *inode, struct file *file)
{
    struct chardev_data *data = container_of(inode->i_cdev, struct chardev_data, cdev);
    
    file->private_data = data;
    
    atomic_inc(&data->open_count);
    printk(KERN_INFO "chardev: Device opened (open count: %d)\n", 
           atomic_read(&data->open_count));
    
    return 0;
}

// Функция закрытия устройства
static int device_release(struct inode *inode, struct file *file)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    
    if (data) {
        atomic_dec(&data->open_count);
        printk(KERN_INFO "chardev: Device closed (open count: %d)\n", 
               atomic_read(&data->open_count));
    }
    
    return 0;
}

// Функция чтения из устройства
static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t count, loff_t *offset)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    ssize_t bytes_to_read;
    
    if (mutex_lock_interruptible(&data->buffer_mutex))
        return -ERESTARTSYS;
    
    if (*offset >= data->buffer_size) {
        mutex_unlock(&data->buffer_mutex);
        return 0;
    }
    
    bytes_to_read = min((size_t)(data->buffer_size - *offset), count);
    
    if (copy_to_user(user_buf, data->buffer + *offset, bytes_to_read)) {
        mutex_unlock(&data->buffer_mutex);
        return -EFAULT;
    }
    
    *offset += bytes_to_read;
    
    mutex_unlock(&data->buffer_mutex);
    
    printk(KERN_DEBUG "chardev: Read %zd bytes from device\n", bytes_to_read);
    return bytes_to_read;
}

// Функция записи в устройство
static ssize_t device_write(struct file *file, const char __user *user_buf,
                           size_t count, loff_t *offset)
{
    struct chardev_data *data = (struct chardev_data *)file->private_data;
    ssize_t bytes_to_write;
    
    if (mutex_lock_interruptible(&data->buffer_mutex))
        return -ERESTARTSYS;
    
    bytes_to_write = min(count, (size_t)DEVICE_BUFFER_SIZE);
    
    if (copy_from_user(data->buffer, user_buf, bytes_to_write)) {
        mutex_unlock(&data->buffer_mutex);
        return -EFAULT;
    }
    
    data->buffer_size = bytes_to_write;
    *offset = bytes_to_write;
    
    mutex_unlock(&data->buffer_mutex);
    
    printk(KERN_DEBUG "chardev: Written %zd bytes to device\n", bytes_to_write);
    
    return bytes_to_write;
}

// Функция инициализации модуля
static int __init char_device_init(void)
{
    dev_t dev_num;
    int ret;
    
    printk(KERN_INFO "chardev: Initializing character device\n");
    
    // Выделяем память для данных устройства
    device_data = kzalloc(sizeof(struct chardev_data), GFP_KERNEL);
    if (!device_data) {
        printk(KERN_ERR "chardev: Failed to allocate device data\n");
        return -ENOMEM;
    }
    
    // Получаем динамический major number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to allocate device number\n");
        kfree(device_data);
        return ret;
    }
    
    major_number = MAJOR(dev_num);
    
    // Инициализируем структуру cdev
    cdev_init(&device_data->cdev, &fops);
    device_data->cdev.owner = THIS_MODULE;
    
    // Добавляем character device в систему
    ret = cdev_add(&device_data->cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "chardev: Failed to add character device\n");
        goto error_cdev_add;
    }
    
    // Создаем класс устройства
    char_class = class_create("chardev_class");
    if (IS_ERR(char_class)) {
        ret = PTR_ERR(char_class);
        printk(KERN_ERR "chardev: Failed to create device class: %d\n", ret);
        goto error_class_create;
    }
    
    // Создаем устройство в /dev и проверяем результат
    char_device = device_create(char_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(char_device)) {
        ret = PTR_ERR(char_device);
        printk(KERN_ERR "chardev: Failed to create device: %d\n", ret);
        goto error_device_create;
    }
    
    // Инициализируем данные устройства
    device_data->buffer_size = 0;
    atomic_set(&device_data->open_count, 0);
    mutex_init(&device_data->buffer_mutex);
    memset(device_data->buffer, 0, DEVICE_BUFFER_SIZE);
    
    printk(KERN_INFO "chardev: Character device registered successfully\n");
    printk(KERN_INFO "chardev: Major number = %d\n", major_number);
    printk(KERN_INFO "chardev: Device created: /dev/%s\n", DEVICE_NAME);
    
    return 0;

// Обработка ошибок с правильной последовательностью очистки
error_device_create:
    class_destroy(char_class);
error_class_create:
    cdev_del(&device_data->cdev);
error_cdev_add:
    unregister_chrdev_region(dev_num, 1);
    kfree(device_data);
    return ret;
}

// Функция выгрузки модуля
static void __exit char_device_exit(void)
{
    dev_t dev_num = MKDEV(major_number, 0);
    
    printk(KERN_INFO "chardev: Unloading character device\n");
    
    // Удаляем устройство из /dev (только если оно было создано)
    if (!IS_ERR_OR_NULL(char_device)) {
        device_destroy(char_class, dev_num);
    }
    
    // Удаляем класс (только если он был создан)
    if (!IS_ERR_OR_NULL(char_class)) {
        class_destroy(char_class);
    }
    
    // Удаляем character device
    if (device_data) {
        cdev_del(&device_data->cdev);
    }
    
    // Освобождаем device number
    unregister_chrdev_region(dev_num, 1);
    
    // Освобождаем память
    if (device_data) {
        kfree(device_data);
    }
    
    printk(KERN_INFO "chardev: Character device unregistered\n");
}

// Регистрация функций
module_init(char_device_init);
module_exit(char_device_exit);
