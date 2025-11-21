# Отчет: Keylogger модуль ядра Linux

## Часть 1: Input Subsystem в ядре

### 1.1 Получение событий от устройств ввода

**Подсистема:** Linux Input Subsystem  
**Тип событий для клавиатуры:** `EV_KEY`

Input Subsystem обрабатывает все события от устройств ввода и передает их зарегистрированным обработчикам.

### 1.2 Структура данных событий

**Структура:** `struct input_event`

**Важные поля:**
- `type` - тип события (`EV_KEY` для клавиатуры)
- `code` - код клавиши (`KEY_A`, `KEY_ENTER` и т.д.)
- `value` - состояние клавиши

### 1.3 Состояния клавиши

- `value == 1` - **нажатие клавиши** (key press)
- `value == 0` - **отпускание клавиши** (key release)
- `value == 2` - **автоповтор** при удержании (key repeat)

**В коде:**
```c
if (type == EV_KEY && value == 1) {
    // Обрабатываем только нажатия
}
```

---

## Часть 2: Регистрация обработчика

### 2.1 Структура обработчика

**Структура:** `struct input_handler`

**Ключевые поля:**
- `.name` - имя обработчика
- `.event` - callback функция для обработки событий
- `.connect` - функция подключения к устройству
- `.disconnect` - функция отключения
- `.id_table` - таблица поддерживаемых устройств

### 2.2 Типы событий

**Бит для клавиатурных событий:** `BIT_MASK(EV_KEY)`

```c
static const struct input_device_id keylog_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_KEY) },
    },
    { },
};
```

### 2.3 Функции регистрации

**Функция регистрации:** `input_register_handler()`  
**Функция отмены регистрации:** `input_unregister_handler()`

### 2.4 Функции модуля

**При загрузке модуля:** `module_init(keylogger_init)`  
**При выгрузке модуля:** `module_exit(keylogger_exit)`

---

## Часть 3: Хранение перехваченных данных

### 3.1 Выделение памяти

**Решение:** Динамическая память  
**Функция выделения:** `kzalloc(BUFFER_SIZE, GFP_KERNEL)`

```c
key_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
```

### 3.2 Структура данных

**Структура:** Кольцевой буфер (circular buffer)  
**При переполнении:** Старые данные перезаписываются новыми

```c
buffer_head = (buffer_head + 1) % BUFFER_SIZE;
if (buffer_count < BUFFER_SIZE) {
    buffer_count++;
} else {
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
}
```

### 3.3 Потокобезопасность

**Механизм синхронизации:** `spinlock`  
**Почему spinlock, а не mutex:**
- Callback вызывается в interrupt context
- В interrupt context нельзя засыпать
- Spinlock работает без блокировки потока

```c
static DEFINE_SPINLOCK(buffer_lock);
spin_lock_irqsave(&buffer_lock, flags);
// критическая секция
spin_unlock_irqrestore(&buffer_lock, flags);
```

### 3.4 Конвертация key codes

**Выбранный вариант:** Сохранение ASCII символов  
**Почему проще:** Удобнее для чтения, не требует дополнительной обработки в userspace

```c
static const char *keycode_to_string[] = {
    [KEY_A] = "a",
    [KEY_ENTER] = "[ENTER]\n",
    // ...
};
```

---

## Часть 4: Чтение через /proc

### 4.1 Создание /proc файла

**Функция создания:** `proc_create()`  
**Функция удаления:** `proc_remove()`

```c
proc_entry = proc_create(PROC_NAME, 0444, NULL, &keylog_proc_ops);
```

### 4.2 Callback для чтения

**Функция чтения:** `keylog_read()`  
**Возвращаемое значение:** Количество прочитанных байт или код ошибки

```c
static ssize_t keylog_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
```

### 4.3 Передача данных в userspace

**Функция копирования:** `copy_to_user()`  
**Проверка указателей:** Функция возвращает количество НЕ скопированных байт (0 = успех)

```c
if (copy_to_user(buf, temp_buf, len))
    ret = -EFAULT;
```

---

## Часть 5: Архитектура и реализация

### 5.1 Kernel-space vs Userspace

**Преимущества kernel-space:**
- Доступ к низкоуровневым событиям до обработки
- Невозможно обойти защиту из userspace
- Работает на уровне всей системы независимо от приложений

**Ограничения userspace:**
- Требуются права root для доступа к `/dev/input/eventX`
- Легко обнаружить через `ps` и `/proc`
- Можно заблокировать через SELinux/AppArmor

### 5.2 Необходимые заголовочные файлы

**Для модулей:**
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
```

**Для input subsystem:**
```c
#include <linux/input.h>
```

**Для /proc:**
```c
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
```

### 5.3 Порядок действий

**При `insmod`:**
1. Выделение памяти (`kzalloc`)
2. Создание `/proc/keylog` (`proc_create`)
3. Регистрация input handler (`input_register_handler`)
4. Подключение к клавиатурным устройствам (`keylog_connect`)

**При `rmmod`:**
1. Отмена регистрации handler (`input_unregister_handler`)
2. Удаление `/proc/keylog` (`proc_remove`)
3. Освобождение памяти (`kfree`)

---

## Часть 6: Обнаружение и защита

### 6.1 Обнаружение keylogger

**Команда для просмотра модулей:**
```bash
lsmod | grep keylog
```

**Поиск /proc записей:**
```bash
ls -la /proc/ | grep keylog
```

**Проверка input handlers:**
```bash
cat /proc/bus/input/handlers
```

**Дополнительно:**
```bash
dmesg | grep keylogger
cat /sys/module/keylogger/version
```

### 6.2 Защита от kernel keyloggers

**На уровне загрузки ядра:**
- Secure Boot с проверкой подписей модулей
- Параметр ядра `module.sig_enforce=1`

**Проверка целостности:**
- IMA (Integrity Measurement Architecture)
- EVM (Extended Verification Module)

**Мониторинг загрузки модулей:**
- `auditd` для логирования вызовов `init_module`
- SELinux/AppArmor политики для ограничения загрузки модулей
