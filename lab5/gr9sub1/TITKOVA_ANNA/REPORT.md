# Лабораторная 5 — Модули ядра Linux

# Вариант 1 (нечётные номера)

## Задание A: Hello World модуль

При загрузке выводит сообщение

```bash
static int __init hello_init(void)  // Функция инициализации модуля
{
    printk(KERN_INFO "hello_module: Hello from Titkova Anna module!\n");  // Строка 32
}
```

При выгрузке выводит сообщение
```bash
static void __exit hello_exit(void)  // Функция выгрузки модуля
{
    printk(KERN_INFO "hello_module: Goodbye from Titkova Anna module!\n");  // Строка 39
}
```
Принимает параметр message
```bash
static char *message = NULL;  // Объявление параметра
module_param(message, charp, 0644);  // Регистрация параметра, строка 22
```
Выводит параметр вместо дефолтного сообщения
```bash
if (message) {
    printk(KERN_INFO "hello_module: %s\n", message);  // Строка 29
} else {
    printk(KERN_INFO "hello_module: Hello from Titkova Anna module!\n");  // Строка 31
}
```
Использует printk с KERN_INFO
Все вызовы printk в файле (строки 29, 31, 39)

Правильные метаданные:
```bash
MODULE_LICENSE("GPL");  // Строка 44
MODULE_AUTHOR("Titkova Anna <anna.titkova@example.com>");  // Строка 45
MODULE_DESCRIPTION("Simple Hello World kernel module");  // Строка 46
MODULE_VERSION("1.0");  // Строка 47
```
module_param для параметра
```bash
module_param(message, charp, 0644);  // Строка 22
MODULE_PARM_DESC(message, "Custom greeting message");  // Строка 23
```
## Быстрый старт
```bash
### Терминал 1

# Следим за логами ядра в реальном времени
sudo dmesg -w


### Терминал 2

# Переходим в папку и собираем модуль
cd src && make hello

# Загружаем без параметров
sudo insmod hello_module.ko

sudo rmmod hello_module

# Загружаем с параметром
sudo insmod hello_module.ko message="Hello!!!"

# Выгружаем
sudo rmmod hello_module
```

## Выводы
Сообщения при загрузке/выгрузке - выводятся как требуется
Имя студента - "Titkova Anna" присутствует в сообщениях
Работа с KERN_INFO - все сообщения имеют правильный уровень
Нет kernel panic - система стабильна
Динамическая загрузка/выгрузка - модуль работает корректно


## Задание B: /proc файл с информацией

Создаёт файл /proc/student_info
```bash
// Создает файл /proc/student_info с правами 0444 (только чтение)
proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);  // Строка 69
```
Выводит имя студента
```bash
"Name: Titkova Anna\n"  // Строка 50 в snprintf
```
Выводит группу и подгруппу
```bash
"Group: 9, Subgroup: 1\n"  // Строка 51 в snprintf
```
Текущее время загрузки в jiffies
```bash
load_time = jiffies;  // Строка 62 (сохранение при загрузке)
"Module loaded at: %lu jiffies\n"  // Строка 52 (вывод в файл)
```
Счётчик обращений к файлу
```bash
static int read_count = 0;  // Строка 17 (объявление счетчика)
read_count++;  // Строка 46 (увеличение при каждом чтении)
"Read count: %d\n"  // Строка 53 (вывод счетчика)
```
Использует proc_create() и proc_remove()
```bash
proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);  // Строка 69 (создание)
proc_remove(proc_file);  // Строка 82 (удаление)
```
Реализует функцию чтения proc_read
```bash
// Функция вызывается при чтении файла /proc/student_info, формирует и возвращает данные
static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) // Строка 40              
```
Использование copy_to_user()
```bash
// Копирование данных из пространства ядра в пространство пользователя
if (copy_to_user(ubuf, buf, len))  // Строка 58
    return -EFAULT;
```
Правильные метаданные модуля
```bash
MODULE_LICENSE("GPL");  // Строка 88
MODULE_AUTHOR("Titkova Anna");  // Строка 89
MODULE_DESCRIPTION("Proc filesystem example");  // Строка 90
MODULE_VERSION("1.0");  // Строка 91
```
## Быстрый старт
```bash
 ### Терминал 1

sudo dmesg -w


 ### Терминал 2

# Сборка и загрузка модуля
cd src && make proc
sudo insmod proc_module.ko

# Проверка создания файла
ls -la /proc/student_info

# Чтение файла 3 раза (обратите внимание на счетчик)
cat /proc/student_info
cat /proc/student_info
cat /proc/student_info

# Выгрузка модуля и проверка удаления файла
sudo rmmod proc_module
ls -la /proc/student_info 2>/dev/null || echo "Файл удален"
```

## Выводы
Модуль proc_module успешно создает файл /proc/student_info с требуемой информацией. При каждом чтении файла счетчик увеличивается, демонстрируя работу глобальной переменной read_count. Файл корректно создается при загрузке модуля и удаляется при выгрузке.
## Задание C: Простой character device

Создаёт character device /dev/mychardev
```bash
ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);  // Строка 75 (выделение номеров)
cdev_init(&my_cdev, &fops);  // Строка 82 (инициализация cdev)
ret = cdev_add(&my_cdev, dev_num, 1);  // Строка 85 (добавление в систему)
```
Устройство можно открыть/закрыть
```bash
static int dev_open(struct inode *inode, struct file *file)  // Строка 88
static int dev_release(struct inode *inode, struct file *file)  // Строка 94
```
При записи сохраняет данные в kernel buffer (1024 байта)
```bash
#define BUF_SIZE 1024  // Строка 14 (максимальный размер буфера)
static char device_buffer[BUF_SIZE];  // Строка 19 (буфер в kernel-space)
```
При чтении возвращает сохранённые данные
```bash
// Читает данные из device_buffer и передает в пользовательское пространство
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)  // Строка 99
```
Выводит в dmesg при открытии/закрытии
```bash
printk(KERN_INFO "chardev: Device opened\n");  // Строка 90
printk(KERN_INFO "chardev: Device closed\n");  // Строка 96
```
Использует alloc_chrdev_region()
```bash
// Динамическое выделение диапазона номеров устройств
ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);  // Строка 75
```
Использует cdev_init() и cdev_add()
```bash
// Инициализация структуры cdev и регистрация устройства в системе
cdev_init(&my_cdev, &fops);  // Строка 82
ret = cdev_add(&my_cdev, dev_num, 1);  // Строка 85
```
Реализует функции операций
```bash
// Определены все требуемые файловые операции: open, release, read, write
static struct file_operations fops = {  // Строка 157
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};
```
Использует copy_from_user()
```bash
// Копирование данных из пользовательского пространства в kernel buffer
if (copy_from_user(device_buffer, buf, bytes_to_write))  // Строка 133
    return -EFAULT;
```
Использует copy_to_user()
```bash
// Копирование данных из kernel buffer в пользовательское пространство
if (copy_to_user(buf, device_buffer + *off, bytes_to_read))  // Строка 109
    return -EFAULT;
```
Ограничение размера записи
```bash
// Гарантирует, что записывается не более 1024 байт
bytes_to_write = min(len, (size_t)BUF_SIZE);  // Строка 126
```
Правильные метаданные модуля
```bash
MODULE_LICENSE("GPL");  // Строка 173
MODULE_AUTHOR("Titkova Anna");  // Строка 174
MODULE_DESCRIPTION("Simple character device driver");  // Строка 175
MODULE_VERSION("1.0");  // Строка 176
```
Корректная очистка ресурсов
```bash
// Удаление cdev и освобождение номеров устройств при выгрузке модуля
cdev_del(&my_cdev);  // Строка 167
unregister_chrdev_region(dev_num, 1);  // Строка 170
```
 Логирование операций записи
```bash
printk(KERN_INFO "chardev: Wrote %d bytes\n", bytes_to_write);  // Строка 138
```

## Быстрый старт
```bash
### Терминал 1

# Следим за логами ядра в реальном времени
sudo dmesg -w

### Терминал 2

# 1. Сборка и загрузка модуля
cd src && make chardev
sudo insmod chardev_module.ko

# 2. Получение major номера
MAJOR=$(sudo dmesg | grep "major number" | tail -1 | awk '{print $NF}')
echo "Major номер: $MAJOR"

# 3. Создание устройства
sudo mknod /dev/mychardev c $MAJOR 0
sudo chmod 666 /dev/mychardev

# 4. Тест записи
echo "Данные для записи" > /dev/mychardev

# 5. Тест чтения
cat /dev/mychardev

# 6. Тест перезаписи
echo "Новые данные" > /dev/mychardev
cat /dev/mychardev

# 7. Очистка
sudo rm -f /dev/mychardev
sudo rmmod chardev_module
```

## Выводы

Модуль chardev_module успешно создает character device с динамическим выделением major номера (240). Устройство корректно открывается и закрывается, операции записи работают (подтверждено логированием в dmesg).

