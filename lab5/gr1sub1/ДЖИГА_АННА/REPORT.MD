# Лабораторная работа №5: Модули ядра Linux

**Студент:** Джи́га Анна  
**Группа:** 1  
**Подгруппа:** 1  
**Номер по списку:** 6  
**Вариант:** 2  

## Цель работы
Изучить архитектуру ядра Linux, научиться разрабатывать, собирать, загружать и отлаживать простые модули ядра, а также освоить механизмы взаимодействия модулей с пространством пользователя.

---

## Ход работы

### Задание A: Модуль "Hello, World!"

#### 1. Описание задания
Создать простой модуль ядра, который:
- При загрузке (`insmod`) выводит в системный журнал: `Hello from Hanna module!`
- При выгрузке (`rmmod`) выводит: `Goodbye from Hanna module!`
- Принимает строковый параметр `message`. Если задан — выводит его вместо стандартного приветствия.

#### 2. Исходный код
Исходный код находится в файле [`./src/hello_module.c`](./src/hello_module.c)

#### 3. Процесс сборки и тестирования
**Сборка:**
```bash
cd src/
make check
make
```

Вывод `make check`:
```
=== Environment Check ===
Kernel version: 5.15.0-139-generic
Kernel headers: ✓ Found at /lib/modules/5.15.0-139-generic/build
Build tools: ✓ gcc installed
Module loading: ✓ insmod available
```

Вывод `make` (сокращён):
```
make -C /lib/modules/5.15.0-139-generic/build M=/home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src modules
  CC [M]  /home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src/hello_module.o
  LD [M]  /home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src/hello_module.ko
```

**Проверка метаданных:**
```bash
modinfo hello_module.ko
```
Вывод:
```
filename:       /home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src/hello_module.ko
version:        1.0
description:    Task A: A simple Hello World kernel module with a parameter.
author:         Hanna
license:        GPL
vermagic:       5.15.0-139-generic SMP mod_unload modversions 
parm:           message:The message to display when the module is loaded. (charp)
```

**Тест без параметра:**
```bash
sudo insmod ./hello_module.ko
lsmod | grep hello_module
sudo dmesg | tail -1
sudo rmmod hello_module
sudo dmesg | tail -1
```
Результаты:
- `lsmod` → `hello_module           16384  0`
- Первый `dmesg` → `[ 3415.872353] hello_module: Hello from Hanna module!`
- Второй `dmesg` → `[ 3490.275234] hello_module: Goodbye from Hanna module!`

**Тест с параметром:**
```bash
sudo insmod ./hello_module.ko message="Yo-ho-ho"
sudo dmesg | tail -1
sudo rmmod hello_module
sudo dmesg | tail -1
```
Результаты:
- `dmesg` → `[ 3546.881227] hello_module: Yo-ho-ho`
- После выгрузки → `[ 3598.531676] hello_module: Goodbye from Hanna module!`

✅ Модуль работает корректно.

---

### Задание B: `/proc` файл с записью

#### 1. Описание задания  
Создать модуль ядра, регистрирующий `/proc/my_config`, который:  
- Поддерживает чтение (`cat`) и запись (`echo`)  
- При чтении возвращает текущее сохранённое значение (по умолчанию — `"default"`)  
- При записи обновляет значение (макс. длина — 256 символов)  
- Обеспечивает потокобезопасность при доступе  

#### 2. Исходный код  
Исходный код находится в файле [`./src/proc_module.c`](./src/proc_module.c)

#### 3. Процесс сборки и тестирования  

**Сборка:**  
```bash
cd src/
make check
make
```

Вывод `make check`:  
```
=== Environment Check ===
Kernel version: 5.15.0-139-generic
Kernel headers: ✓ Found at /lib/modules/5.15.0-139-generic/build
Build tools: ✓ gcc installed
Module loading: ✓ insmod available
```

**Проверка метаданных:**  
```bash
modinfo proc_module.ko
```  
Вывод:  
```
filename:       /home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src/proc_module.ko
version:        1.1
description:    Task B: Read/write /proc entry with thread safety.
author:         Hanna
license:        GPL
vermagic:       5.15.0-139-generic SMP mod_unload modversions
```

**Тестирование:**  
```bash
sudo insmod ./proc_module.ko
ls -l /proc/my_config
cat /proc/my_config
echo "Lalala" > /proc/my_config
cat /proc/my_config
sudo rmmod proc_module
ls -l /proc/my_config
```

Результаты:  
- После загрузки: `/proc/my_config` существует с правами `-rw-rw-rw-`  
- Первое чтение: `default`  
- После записи `echo "Lalala"`: чтение возвращает `Lalala`  
- После `rmmod`: файл `/proc/my_config` отсутствует (`No such file or directory`)  

✅ Модуль корректно создаёт, читает, обновляет и удаляет `/proc`-файл.

---

### Задание C: `/proc` файл со статистикой системы

#### 1. Описание задания  
Создать read-only `/proc/sys_stats`, который при чтении выводит:  
- Количество процессов в состоянии `TASK_RUNNING`  
- Используемую оперативную память в мегабайтах  
- Время работы системы (uptime) в секундах  

#### 2. Исходный код  
Исходный код находится в файле [`./src/sys_stats_module.c`](./src/sys_stats_module.c)

#### 3. Процесс сборки и тестирования  

**Сборка:**  
```bash
cd src/
make
```

**Проверка метаданных:**  
```bash
modinfo sys_stats_module.ko
```  
Вывод:  
```
filename:       /home/user/lab/lab5/gr1sub1/ДЖИГА_АННА/src/sys_stats_module.ko
version:        1.1
description:    Task C: Shows system stats via /proc/sys_stats.
author:         Hanna
license:        GPL
vermagic:       5.15.0-139-generic SMP mod_unload modversions
```

**Тестирование:**  
```bash
sudo insmod ./sys_stats_module.ko
cat /proc/sys_stats
sudo rmmod sys_stats_module
ls -l /proc/sys_stats
```

Результаты:  
- Вывод `cat /proc/sys_stats`:  
  ```
  Processes: 3
  Memory Used: 1867 MB
  System Uptime: 4632 seconds
  ```  
- После выгрузки: файл `/proc/sys_stats` отсутствует (`No such file or directory`)  

✅ Модуль корректно предоставляет актуальную системную статистику через `/proc`.

---

## Ответы на вопросы

### Базовые понятия
**Что такое модуль ядра и зачем он нужен?**  
Модуль ядра — это фрагмент кода, который можно динамически загружать и выгружать из ядра Linux без перезагрузки системы. Он нужен для расширения функциональности ядра (например, добавления драйверов, файловых систем) без перекомпиляции всего ядра.

**Чем отличается kernel-space от user-space?**  
User-space — непривилегированное пространство обычных приложений; сбой влияет только на процесс. Kernel-space — привилегированное пространство ядра; ошибки здесь могут вызвать kernel panic и остановку всей системы.

**Что произойдёт, если в модуле обратиться к NULL указателю?**  
Произойдёт page fault в kernel-space, что немедленно вызовет kernel panic.

**Почему нельзя использовать `printf()` в модуле ядра?**  
Потому что `printf()` — часть libc (user-space). В ядре используется `printk()`, которая пишет в кольцевой буфер ядра.

**Что такое kernel panic и как его избежать?**  
Kernel panic — фатальный сбой ядра. Избежать его можно: проверкой указателей, корректным управлением памятью, использованием `copy_to/from_user`, избеганием блокирующих операций в неподходящих контекстах.

### Жизненный цикл модуля
**Какие функции вызываются при `insmod` и `rmmod`?**  
`insmod` → функция, помеченная `module_init()`.  
`rmmod` → функция, помеченная `module_exit()`.

**Что должна делать функция `module_exit()`?**  
Освобождать все ресурсы: удалять `/proc` файлы, вызывать `kfree()`, отменять регистрацию устройств и т.п.

**Что происходит, если `module_init()` возвращает ошибку?**  
Модуль не загружается, `module_exit()` не вызывается, и все уже выделенные ресурсы должны быть освобождены внутри `module_init()` до возврата ошибки.

**Можно ли выгрузить модуль, если он используется?**  
Нет. Ядро отслеживает счётчик использования; если он > 0, `rmmod` завершится с ошибкой.

### Логирование и отладка
**Чем `printk()` отличается от `printf()`?**  
`printk()` работает в ядре, пишет в лог ядра, поддерживает уровни приоритета (`KERN_INFO`, `KERN_ERR` и т.д.).

**Как посмотреть логи модуля?**  
Командой `dmesg` или `journalctl -k`.

### Память и безопасность
**Чем `kmalloc()` отличается от `malloc()`?**  
`kmalloc()` выделяет физически непрерывную память в kernel-space с флагами GFP.

**Почему нельзя использовать user-space указатели напрямую в ядре?**  
Потому что память user-space может быть выгружена или недоступна; прямой доступ вызовет page fault в kernel-space → panic.

**Зачем нужны `copy_to_user()` и `copy_from_user()`?**  
Для безопасного копирования данных между пространствами с проверкой адресов и обработкой ошибок.

### Параметры и метаданные
**Как передать параметры модулю?**  
Через `insmod module.ko param=value`. В коде — макрос `module_param()`.

**Зачем нужен `MODULE_LICENSE("GPL")`?**  
Некоторые ядерные функции доступны только GPL-модулям. Без указания лицензии ядро помечается как "tainted".

### Практические вопросы
**Как узнать загруженные модули?**  
`lsmod` или `cat /proc/modules`.

**Как получить информацию о модуле?**  
`modinfo module.ko`.

---

### Выводы

В ходе выполнения лабораторной работы №5 были освоены ключевые аспекты разработки модулей ядра Linux:
- Реализованы три модуля, демонстрирующие базовую функциональность: приветствие с параметром, двусторонний обмен данными через `/proc` и экспорт системной статистики.
- Изучены механизмы регистрации `/proc`-файлов, передачи параметров, безопасной работы с памятью и взаимодействия с пользовательским пространством.
- Подтверждена работоспособность модулей на ядре `5.15.0-139-generic` с корректной загрузкой, работой и выгрузкой без сбоев.
- Отработаны навыки отладки с помощью `dmesg`, `lsmod`, `modinfo` и проверки состояния `/proc`.

Работа выполнена в полном соответствии с требованиями Варианта 2.

(Также предоставлены скриншоты в папке screenshots)

---

### Использование AI

AI использовался для форматирования отчёта, генерации структуры `REPORT.md` и `README.md`.


