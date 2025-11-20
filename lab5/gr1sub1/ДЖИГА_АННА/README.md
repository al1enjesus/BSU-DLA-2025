# Лабораторная работа №5 — Модули ядра Linux  
**Автор:** Анна Джига  
**Ядро:** `5.15.0-139-generic`

## Содержание модулей
- `hello_module.ko` — Простой Hello World модуль с параметром `message`.
- `proc_module.ko` — Чтение/запись через `/proc/my_config` (макс. 256 символов, потокобезопасность).
- `sys_stats_module.ko` — Системная статистика в `/proc/sys_stats` (процессы, память, uptime).

## Требования
- ОС: Linux (Ubuntu 20.04/22.04 или аналог)
- Ubuntu/Debian с ядром `5.15.0-139-generic`
- Установленные заголовки ядра:  
  ```bash
  sudo apt install linux-headers-$(uname -r)
  ```
- Утилиты: `gcc`, `make`, `sudo`
- Окружение: ⚠️ Обязательно виртуальная машина!

## Сборка
```bash
cd src/
make check   # Проверка окружения
make         # Сборка всех модулей
```

## Запуск и тестирование

### 1. Hello Module
```bash
sudo insmod hello_module.ko
dmesg | tail -1  # → Hello from Hanna module!

sudo insmod hello_module.ko message="Yo-ho-ho"
dmesg | tail -1  # → Yo-ho-ho

sudo rmmod hello_module
```

### 2. Proc Module (read/write)
```bash
sudo insmod proc_module.ko
cat /proc/my_config                 # → default
echo "Lalala" > /proc/my_config
cat /proc/my_config                 # → Lalala
sudo rmmod proc_module              # Файл /proc/my_config удаляется
```

### 3. System Stats Module
```bash
sudo insmod sys_stats_module.ko
cat /proc/sys_stats
# Пример вывода:
# Processes: 3
# Memory Used: 1867 MB
# System Uptime: 4632 seconds

sudo rmmod sys_stats_module         # Файл /proc/sys_stats удаляется
```

## Очистка
```bash
make clean  # Удаляет .ko, .o и временные файлы
```

> ⚠️ **Важно**: Модули скомпилированы **только для ядра 5.15.0-139-generic**. На других версиях ядра они не загрузятся без пересборки.
