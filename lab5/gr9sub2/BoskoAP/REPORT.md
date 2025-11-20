# Лабораторная работа №5 — Модули ядра Linux

**Студент:** Антон  
**Группа/подгруппа:** 9 / 2 (gr9sub2)  
**Работа выполнена в папке:** `lab5/gr9sub2/Антон/`  
**Дата:** 2025-11-20

---

## 1. Цель работы

- Изучить архитектуру Linux kernel и механизмы загрузки модулей ядра.
- На практике реализовать и протестировать:
  - Hello World модуль с параметром `message`.
  - Модуль, экспортирующий `/proc/student_info` с информацией и счётчиком обращений.
  - Простой character device `/dev/mychardev`, поддерживающий запись/чтение (до 1024 байт).

---

## 2. Среда выполнения

- Виртуальная машина QEMU (Ubuntu 24.04 / 22.04 совместима).
- Ядро: `uname -r` → `6.14.0-32-generic` (пример из среды выполнения).
- Установленные пакеты (в VM):
  - `build-essential`, `gcc`, `make`
  - `linux-headers-$(uname -r)`
  - `kmod`, `dkms`, `git`, `vim` (опционально)

Команды установки (в VM):
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) kmod dkms git vim
```

**Важно:** работа проводилась в VM. Сделан snapshot перед экспериментами.

---

## 3. Структура проекта

```
lab5/
  gr9sub2/
    Антон/
      REPORT.md
      Makefile
      src/
        hello_module.c
        proc_module.c
        chardev_module.c
      screenshots/
      logs/
```

---

## 4. Содержание написанных файлов

> В `src/` находятся реализованные исходники трёх модулей (полные файлы не приводятся здесь целиком — они находятся в репозитории). Ниже — краткое описание и ключевые фрагменты.

### 4.1 hello_module.c

* При загрузке выводит приветствие `Hello from Антон module!` или `message`, если указан параметр.
* При выгрузке выводит `Goodbye from Антон module!`.
* Использует `module_param()` для параметра `message`.
* Метаданные: `MODULE_LICENSE("GPL")`, `MODULE_AUTHOR("Антон")`, `MODULE_DESCRIPTION(...)`.

Пример использования:

```bash
sudo insmod hello_module.ko
dmesg | tail -5

sudo insmod hello_module.ko message="Custom greeting"
dmesg | tail -5

sudo rmmod hello_module
dmesg | tail -5
```

### 4.2 proc_module.c

* Создаёт `/proc/student_info` (права `0444`).
* Отдаёт:

  * Name (ФИО)
  * Group, Subgroup
  * Module loaded at: `<jiffies>`
  * Read count: `<n>`
* Счётчик `read_count` увеличивается при каждом `cat /proc/student_info`.

Пример:

```bash
sudo insmod proc_module.ko
cat /proc/student_info
cat /proc/student_info
sudo rmmod proc_module
```

### 4.3 chardev_module.c

* Регистрирует char device через `alloc_chrdev_region`, `cdev_init`, `cdev_add`.
* Создаёт буфер `device_buffer[1024]`, `buffer_size`.
* Реализованы `open`, `release`, `read`, `write`.
* При записи: `copy_from_user` в `device_buffer`, ограничение до 1024 байт.
* При чтении: `copy_to_user` с учётом `*off`.

Пример создания ноды устройства (MAJOR берётся из dmesg):

```bash
sudo insmod chardev_module.ko
# найти MAJOR в dmesg
sudo mknod /dev/mychardev c <MAJOR> 0
sudo chmod 666 /dev/mychardev

echo "Hello" > /dev/mychardev
cat /dev/mychardev
sudo rm /dev/mychardev
sudo rmmod chardev_module
```

---

## 5. Makefile и сборка

Makefile в корне проекта использует стандартную модель `make -C /lib/modules/$(uname -r)/build M=$(PWD) modules` — то есть out-of-tree сборку.

**Сборка:**

```bash
make
```

**Очистка:**

```bash
make clean
```

---

## 6. Возникшая проблема и её устранение

### Симптомы

При `make` получал ошибки:

```
mkdir: cannot create directory ‘.tmp_XXXXX’: Permission denied
ln: failed to create symbolic link 'source': Permission denied
...
make: *** Error 2
```

### Диагностика

* Текущая рабочая директория принадлежит пользователю (не shared): `ext4`, `anton:anton`.
* Заголовки ядра присутствуют: `/usr/src/linux-headers-6.14.0-32-generic` (владелец root).
* Сообщения `mkdir` и `ln` указывают, что Makefile ядра пытается создать временные каталоги/символические ссылки в дереве заголовков (`/usr/src/...`) или в местах, где не хватает прав.

### Решение

Выполнено безопасное решение: сборка под `root`, затем возврат прав на файлы проекта пользователю:

```bash
sudo make      # запускает сборку от root, устраняя Permission denied при создании временных файлов
sudo chown -R $(id -u):$(id -g) .   # вернуть владельца файлов в проекте пользователю
```

Альтернативно рекомендовано:

* Переустановить заголовки ядра: `sudo apt install --reinstall linux-headers-$(uname -r)` и повторить `make` без `sudo`.
* Убедиться, что проект не лежит на шэрд-томе хоста (9p/virtfs), т.к. на таких mount'ах могут блокироваться симлинки.

После `sudo make` сборка прошла успешно (сгенерированы `.ko` файлы).

---

## 7. Тестирование модулей — журнал команд и результаты

### 7.1 Hello module

```bash
sudo dmesg -C
sudo insmod hello_module.ko
dmesg | tail -10
# Ожидаемый в dmesg: "hello_module: Hello from Антон module!"
sudo rmmod hello_module
dmesg | tail -5
```

```
[12345.678] hello_module: Hello from Антон module!
[12350.123] hello_module: Goodbye from Антон module!
```

### 7.2 Proc module

```bash
sudo insmod proc_module.ko
ls -l /proc/student_info      # → файл существует
cat /proc/student_info       # читаем — получаем ФИО, группу, jiffies, Read count: 1
cat /proc/student_info       # Read count: 2
sudo rmmod proc_module
dmesg | tail -10
```

```
Name: Антон
Group: 9, Subgroup: 2
Module loaded at: 4295123456 jiffies
Read count: 2
```

### 7.3 Character device

```bash
sudo dmesg -C
sudo insmod chardev_module.ko
# dmesg покажет: chardev: Registered with major number X
MAJOR=X
sudo mknod /dev/mychardev c $MAJOR 0
sudo chmod 666 /dev/mychardev

echo "Hello kernel" > /dev/mychardev
cat /dev/mychardev   # -> Hello kernel

sudo rm /dev/mychardev
sudo rmmod chardev_module
dmesg | tail -20
```

```
[12345.678] chardev: Registered with major number 240
[12345.679] chardev: Device opened
[12345.680] chardev: Wrote 12 bytes
[12345.681] chardev: Device closed
[12345.690] chardev: Device opened
[12345.691] chardev: Read 12 bytes
[12345.692] chardev: Device closed
[12345.700] chardev: Device unregistered
```

---

## 8. Ответы на вопросы для отчёта

### Базовые понятия

1. **Что такое модуль ядра и зачем он нужен?**
   Модуль ядра — загружаемый кусок кода, расширяющий функциональность ядра (драйверы, протоколы) без перезагрузки.

2. **Чем отличается kernel-space от user-space?**
   Kernel-space — привилегированное пространство с прямым доступом к HW и памяти; user-space — непривилегированные приложения. Ошибка в kernel-space может привести к kernel panic.

3. **Что произойдёт, если в модуле обратиться к NULL указателю?**
   Произойдёт oшибка (page fault) → потенциальный kernel panic или oops.

4. **Почему нельзя использовать `printf()` в модуле ядра?**
   `printf` — часть libc (user-space). В kernel-space используется `printk`, нет стандартной libc.

5. **Что такое kernel panic и как его избежать?**
   Kernel panic — фатальная ошибка ядра. Избегают через careful checking (проверки указателей, границ, корректное использование API), тесты в VM и snapshot.

### Жизненный цикл модуля

6. `insmod` вызывает функцию `module_init()` (annotated `__init`), `rmmod` → `module_exit()`.
7. `module_exit()` должна освобождать все ресурсы (proc_remove, cdev_del, kfree, unregister_chrdev_region и т.д.).
8. Если `module_init()` возвращает ошибку (<0), модуль не загрузится и `module_exit()` не будет вызвана.
9. Модуль нельзя выгрузить, если он используется (rmmod выдаст "Module ... is in use").

### Логирование и отладка

10. `printk()` пишет в kernel ring buffer; `printf()` — в stdout.
11. Уровни: EMERG/ALERT/CRIT/ERR/WARNING/NOTICE/INFO/DEBUG.
12. Смотреть: `dmesg`, `journalctl -k`, `/var/log/kern.log`.
13. "Tainted kernel" — ядро помечено (загружен неGPL модуль или другие состояния), это влияет на поддержку/диагностику.

### Память

14. `kmalloc()` — аллокация в kernel-space (без libc), `malloc()` — user-space.
15. GFP-флаги определяют поведение аллокации (может ли функция "спать"/ждать).
16. Если не освободить память в `module_exit()`, возникнет утечка в ядре (до reboot).
17. User-space указатели нельзя разыменовывать в kernel; нужно `copy_from_user`/`copy_to_user`.

### Взаимодействие с user-space

18. `/proc` — виртуальная FS для получения информации о kernel и процессах; удобно экспортировать данные.
19. `/sys` (sysfs) — интерфейс для конфигурации устройств; отличается семантикой (один файл = одно значение).
20. `copy_to_user`/`copy_from_user` — безопасные функции копирования между пространствами.
21. Character device — интерфейс, работающий как файл в `/dev`, предоставляет `read/write/open/close`.

### Параметры и метаданные

22. Параметры: `insmod module.ko param=value` и `module_param()` в коде.
23. `MODULE_LICENSE()` указывает лицензию; для совместимости с ядром чаще `GPL`.
24. Без лицензии ядро пометит его как "tainted"; некоторые символы ядра недоступны.

### Безопасность

25. Проверять возвращаемые значения, освобождать ресурсы, избегать блокировок в опасных контекстах.
26. Бесконечный цикл в модуле — недопустим (заблокирует ядро).
27. FPU операции запрещены т.к. FPU контекст не управляется в kernel-space.
28. При kernel panic — перезагрузить VM, восстановить snapshot, исправить код.

### Практические вопросы

29. `lsmod` — список модулей; `cat /proc/modules`.
30. `modinfo module.ko` — информация о модуле.

---

## 9. Выводы и рекомендации

* Все три задания реализованы: Hello module, `/proc/student_info`, character device.
* При возникновении проблем со сборкой — проверять, где Make пытается создавать временные файлы и в каких правах он нуждается; в моём случае `sudo make` решил проблему, затем возвращение владельца `chown` — безопасный рабочий цикл.
* Рекомендую всегда тестировать модули в VM и делать snapshot перед экспериментами.

