# Отчет по лабораторной работе №5 — Модули ядра Linux

**Студент:** Пардаев Алексей  
**Группа:** 9, подгруппа 1  
**Вариант:** 1

---

## Выполненные задания

### Задание A: Hello World модуль

**Описание:**  
Создан модуль `hello_module.c`, выполняющий следующие функции:

- При загрузке выводит сообщение «Hello from Pardaev Aleksey module!»
- При выгрузке — «Goodbye from Pardaev Aleksey module!»
- Поддерживает параметр `message`, позволяющий задать пользовательское приветствие
- Содержит корректные метаданные (`author`, `license`, `description`, `version`)

**Результаты тестирования:**

```
$ make test-hello
=== Testing hello_module ===
Loading module...
Output:
[ 2351.182392] hello_module: Hello from Pardaev Aleksey module!
Unloading module...
Output:
[ 2351.243421] hello_module: Goodbye from Pardaev Aleksey module!
```

**Тест с параметром:**

```
$ sudo insmod hello_module.ko message="Hi!"
$ dmesg | tail -1
[ 2402.190874] hello_module: Hi!
```

---

### Задание B: /proc файл с информацией

**Описание:**  
Создан модуль `proc_module.c`, который:

- Создает файл `/proc/student_info`
- Выводит:
    - Имя студента
    - Группу и подгруппу
    - Время загрузки модуля в тиках jiffies
    - Количество обращений к файлу
- Корректно удаляет файл при выгрузке

**Результаты тестирования:**

```
$ make test-proc
=== Testing proc_module ===
Loading module...
Reading /proc/student_info:
Name: Pardaev Aleksey
Group: 9, Subgroup: 1
Module loaded at: 4303771965 jiffies
Read count: 1
dmesg output:
[ 2410.152773] proc_module: Initializing
[ 2410.152810] proc_module: Created /proc/student_info
[ 2410.195880] proc_module: Removed /proc/student_info
[ 2410.195884] proc_module: Exiting. Total reads: 1
```

---

### Задание C: Простой символьный драйвер (character device)

**Описание:**  
Создан модуль `chardev_module.c`, который:

- Регистрирует символьное устройство `/dev/mychardev`
- Поддерживает операции:
    - `open` — открытие устройства
    - `read` — чтение данных из буфера
    - `write` — запись данных пользователем
    - `release` — закрытие устройства
- Использует буфер размером 1024 байта
- Выводит информацию о действиях через `dmesg`

**Результаты тестирования:**

```
$ make test-chardev
=== Testing chardev_module ===
Loading module...
Creating device node with major 240...
Writing to device...
Reading from device:
Test data
dmesg output:
[ 2420.914101] chardev: Registered with major number 240
[ 2420.966305] chardev: Device opened
[ 2420.966315] chardev: Written 10 bytes
[ 2420.968021] chardev: Read 10 bytes
[ 2420.968084] chardev: Device closed
[ 2420.994520] chardev: Device unregistered
```

---

## Информация о модулях

```bash
$ modinfo hello_module.ko
filename: hello_module.ko
version: 1.0
description: Simple Hello World kernel module
author: Pardaev Aleksey <pardaev@gmail.com>
license: GPL
parm: message:Custom greeting message (charp)

$ modinfo proc_module.ko
filename: proc_module.ko
version: 1.0
description: Proc filesystem example
author: Pardaev Aleksey <pardaev@gmail.com>
license: GPL

$ modinfo chardev_module.ko
filename: chardev_module.ko
version: 1.0
description: Simple character device driver
author: Pardaev Aleksey <pardaev@gmail.com>
license: GPL
```

---

## Выводы

В ходе лабораторной работы были:

- Освоены основы разработки модулей ядра Linux;
- Реализованы и протестированы три типа модулей:
    1. Простой модуль с параметрами (`hello_module`);
    2. Модуль, взаимодействующий с файловой системой `/proc` (`proc_module`);
    3. Символьный драйвер устройства (`chardev_module`);
- Получены навыки сборки модулей через `Makefile`, загрузки с `insmod`, выгрузки с `rmmod`, анализа логов `dmesg`;
- Подтверждена корректная работа всех модулей на целевой системе.
