# Лабораторная работа №5: Модули ядра Linux

**Студент:** Лебедев Денис  
**Группа:** 9, Подгруппа: 2  
**Вариант:** 2 (чётный номер по списку)

---

## Описание

Данная лабораторная работа посвящена изучению разработки модулей ядра Linux. Реализованы три модуля:

1. **hello_module** - простой модуль с приветствием и поддержкой параметров
2. **proc_module** - модуль с файлом `/proc/my_config` для чтения и записи
3. **sys_stats_module** - модуль для отображения системной статистики через `/proc/sys_stats`

---

## Структура проекта

```
Lebedev_Denis/
├── README.md              # Этот файл
├── REPORT.MD              # Подробный отчёт о выполнении
├── src/                   # Исходные коды модулей
│   ├── hello_module.c
│   ├── proc_module.c
│   ├── sys_stats_module.c
│   └── Makefile
└── screenshots/           # Скриншоты выполнения
    ├── task_a.png
    ├── task_b.png
    └── task_c.png
```

---

## Требования

- **ОС:** Linux (Ubuntu 22.04/24.04 или аналог)
- **Ядро:** Linux kernel с headers (проверьте: `ls /lib/modules/$(uname -r)/build`)
- **Инструменты:** `build-essential`, `kmod`
- **Окружение:** ⚠️ **Обязательно виртуальная машина!**

### Установка зависимостей

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) kmod
```

---

## Быстрый запуск

### 1. Проверка окружения

```bash
cd src/
make check
```

Вы должны увидеть сообщения о том, что kernel headers, gcc и kmod найдены.

### 2. Сборка модулей

```bash
make
```

Будут созданы файлы: `hello_module.ko`, `proc_module.ko`, `sys_stats_module.ko`

### 3. Тестирование

#### Задание A: Hello Module

```bash
# Загрузка модуля без параметра
sudo insmod hello_module.ko
dmesg | tail -5

# Загрузка с параметром
sudo rmmod hello_module
sudo insmod hello_module.ko message="Custom message"
dmesg | tail -5

# Выгрузка
sudo rmmod hello_module
dmesg | tail -5
```

**Ожидаемый результат:**

- При загрузке без параметра: "Hello from Lebedev Denis module!"
- При загрузке с параметром: ваше сообщение
- При выгрузке: "Goodbye from Lebedev Denis module!"

#### Задание B: Proc Module

```bash
# Загрузка модуля
sudo insmod proc_module.ko

# Проверка файла
ls -la /proc/my_config

# Чтение (должно вернуть "default")
cat /proc/my_config

# Запись нового значения
echo "New value" > /proc/my_config

# Чтение обновлённого значения
cat /proc/my_config

# Выгрузка
sudo rmmod proc_module

# Проверка, что файл удалён
ls -la /proc/my_config  # Должна быть ошибка
```

**Ожидаемый результат:**

- По умолчанию файл содержит "default"
- После записи содержит новое значение
- После выгрузки модуля файл исчезает

#### Задание C: System Stats Module

```bash
# Загрузка модуля
sudo insmod sys_stats_module.ko

# Чтение статистики
cat /proc/sys_stats

# Выгрузка
sudo rmmod sys_stats_module
```

**Ожидаемый результат:**

```
Processes: N
Memory Used: XXXX MB
System Uptime: YYYY seconds
```

### 4. Очистка

```bash
make clean
```

---

## Проверка загруженных модулей

```bash
# Список всех загруженных модулей
lsmod | grep -E "hello|proc|sys_stats"

# Информация о модуле
modinfo hello_module.ko
modinfo proc_module.ko
modinfo sys_stats_module.ko

# Просмотр логов ядра
dmesg | tail -20
dmesg -w  # В реальном времени
```

---

## Особенности реализации

### Hello Module

- Поддержка параметра `message` для кастомизации приветствия
- Вывод сообщений при загрузке и выгрузке

### Proc Module

- **Thread-safe**: использование mutex для защиты от race conditions
- Поддержка чтения и записи
- Ограничение размера буфера (256 байт)
- Автоматическое удаление символа новой строки при записи

### System Stats Module

- Подсчёт процессов в состоянии `TASK_RUNNING`
- Отображение используемой памяти (в МБ)
- Uptime системы (в секундах)
- Использование RCU lock для безопасной итерации по процессам
