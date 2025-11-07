# Лабораторная работа №5 - Модули ядра Linux

**Выполнил:** Поляков Андрей
**Группа:** 9, подгруппа 1
**Вариант:** 1 (нечётный номер, 19 по списку)

## Описание

Данная лабораторная работа посвящена изучению разработки модулей ядра Linux. В рамках работы реализованы три модуля:

- `hello_module` - простой модуль "Hello World"
- `proc_module` - модуль с /proc файлом для вывода информации о студенте
- `chardev_module` - простой character device драйвер

## Структура проекта

```
lab5/gr9sub1/Polyakov_Andrei/
├── README.md
├── REPORT.md
├── Makefile
├── src/
│   ├── hello_module.c
│   ├── proc_module.c
│   └── chardev_module.c
├── screenshots/ *.png
```

## Системные требования

- Linux kernel 6.8.0-87-generic или совместимый
- Заголовочные файлы ядра (linux-headers-$(uname -r))
- GCC компилятор
- Права администратора для загрузки модулей

## Быстрый старт

### Проверка окружения

```bash
make check
```

### Сборка всех модулей

```bash
make all
```

### Быстрое тестирование модулей

```bash
make test-hello    # Тест hello_module
make test-proc     # Тест proc_module  
make test-chardev  # Тест chardev_module
```

## Описание модулей

### hello_module.c

Простейший модуль ядра, демонстрирующий:

- Инициализацию и очистку модуля
- Использование printk() для логирования
- Работу с параметрами модуля
- Корректные метаданные модуля

**Функциональность:**

- При загрузке выводит приветствие
- При выгрузке выводит прощание
- Поддерживает параметр message для пользовательского сообщения

**Использование:**

```bash
sudo insmod hello_module.ko
sudo insmod hello_module.ko message="Custom message"
sudo rmmod hello_module
dmesg | tail
```

### proc_module.c

Модуль для работы с файловой системой /proc:

- Создает файл /proc/student_info
- Отображает информацию о студенте
- Ведет счетчик обращений к файлу
- Показывает время загрузки модуля в jiffies

**Функциональность:**

- Создание и удаление proc файла
- Реализация операции чтения
- Счетчик обращений
- Использование jiffies для временных меток

**Использование:**

```bash
sudo insmod proc_module.ko
cat /proc/student_info
sudo rmmod proc_module
```

### chardev_module.c

Простой драйвер символьного устройства:

- Создает character device /dev/mychardev
- Поддерживает операции открытия/закрытия
- Реализует чтение и запись данных
- Буферизует до 1024 байт данных

**Функциональность:**

- Динамическое выделение major номера
- Операции open, release, read, write
- Безопасное копирование данных между kernel и user space
- Логирование операций

**Использование:**

```bash
sudo insmod chardev_module.ko
# Узнать major номер из dmesg
sudo mknod /dev/mychardev c <MAJOR> 0
sudo chmod 666 /dev/mychardev
echo "Test data" > /dev/mychardev
cat /dev/mychardev
sudo rm /dev/mychardev
sudo rmmod chardev_module
```

## Makefile

Предоставляет удобные команды для работы с модулями:

### Основные команды

- `make` или `make all` - сборка всех модулей
- `make clean` - очистка временных файлов
- `make check` - проверка окружения
- `make help` - справка по командам

### Работа с модулями

- `make load MODULE=<name>` - загрузка модуля
- `make unload MODULE=<name>` - выгрузка модуля
- `make info` - информация о загруженных модулях

### Автоматическое тестирование

- `make test-hello` - полный тест hello_module
- `make test-proc` - полный тест proc_module
- `make test-chardev` - полный тест chardev_module

## Примеры работы

### Информация о модулях

```bash
$ modinfo hello_module.ko
filename: hello_module.ko
version: 1.0
description: Simple Hello World kernel module
author: Polyakov Andrei <polyakov@example.com>
license: GPL
parm: message:Custom greeting message (charp)
```

### Загруженные модули

```bash
$ lsmod | grep hello_module
hello_module           12288  0
```

### Логи ядра

```bash
$ dmesg | grep hello_module
hello_module: Hello from Polyakov Andrei module!
hello_module: Goodbye from Polyakov Andrei module!
```

## Особенности реализации

- Все модули используют лицензию GPL
- Корректная обработка ошибок во всех критичных местах
- Безопасное копирование данных между kernel и user space
- Правильная очистка ресурсов при выгрузке модулей
- Подробное логирование операций

## Безопасность

- Использование `copy_to_user()` и `copy_from_user()` для безопасного обмена данными
- Проверка возвращаемых значений системных вызовов
- Корректная инициализация и освобождение ресурсов
- Ограничение размера буферов

## Известные ограничения

- Character device поддерживает буфер до 1024 байт
- Proc файл показывает статическую информацию о студенте
- Модули протестированы на Ubuntu 22.04 с ядром 6.8.0-87
