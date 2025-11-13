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


## Вопросы и ответы

### Базовые понятия
1. **Что такое модуль ядра и зачем он нужен?**  
   Модуль ядра — это загружаемый компонент, расширяющий функциональность ядра без пересборки. Используется для драйверов, системных интерфейсов и новых подсистем.

2. **Чем отличается kernel-space от user-space?**  
   Kernel-space — привилегированная область, где работает ядро.  
   User-space — обычные программы.  
   Напрямую они не пересекаются.

3. **Что произойдёт, если в модуле обратиться к NULL указателю?**  
   Возникнет kernel panic (фатальная ошибка ядра).

4. **Почему нельзя использовать `printf()` в модуле ядра?**  
   Потому что это библиотека glibc из user-space.  
   В ядре используется `printk()`.

5. **Что такое kernel panic и как его избежать?**  
   Полная остановка ядра из-за критической ошибки.  
   Избегается валидными указателями, проверками ошибок, корректным управлением ресурсами.

### Жизненный цикл модуля
6. **Какие функции вызываются при `insmod` и `rmmod`?**  
   `insmod`: вызывается функция, помеченная `module_init()`.  
   `rmmod`: вызывается `module_exit()`.

7. **Что должна делать функция `module_exit()`?**  
   Освобождать все ресурсы: память, символы, устройства, /proc файлы.

8. **Что происходит, если `module_init()` возвращает ошибку?**  
   Модуль не загружается.

9. **Можно ли выгрузить модуль, если он используется?**  
   Нет. Ядро покажет «module is in use».

### Логирование и отладка
10. **Чем `printk()` отличается от `printf()`?**  
    `printk()` поддерживает уровни логирования и пишет в буфер ядра.

11. **Какие уровни логирования существуют в ядре?**  
    KERN_EMERG, ALERT, CRIT, ERR, WARNING, NOTICE, INFO, DEBUG.

12. **Как посмотреть логи модуля?**  
    `dmesg | tail`.

13. **Что означает "tainted kernel"?**  
    Ядро загружено с недостоверным или проприетарным модулем.

### Память
14. **Чем `kmalloc()` отличается от `malloc()`?**  
    `kmalloc()` выделяет память внутри ядра и использует флаги GFP.

15. **Что такое флаги GFP и зачем они нужны?**  
    Указывают, как именно следует выделять память (например, блокирующее или атомарное выделение).

16. **Что произойдёт, если не освободить память в `module_exit()`?**  
    Утечка памяти в ядре до перезагрузки.

17. **Почему нельзя использовать user-space указатели напрямую в ядре?**  
    Они недействительны в kernel-space.

### Взаимодействие с user-space
18. **Что такое `/proc` и для чего он используется?**  
    Псевдофайловая система, позволяющая ядру экспортировать информацию.

19. **Что такое `/sys` (sysfs) и чем отличается от procfs?**  
    Sysfs предоставляет объектную модель ядра.  
    Procfs — интерфейс процессов и статистики.

20. **Зачем нужны `copy_to_user()` и `copy_from_user()`?**  
    Для безопасного копирования данных между user-space и kernel-space.

21. **Что такое character device и как он работает?**  
    Символьное устройство — потоковый интерфейс с реализацией file_operations.

### Параметры и метаданные
22. **Как передать параметры модулю при загрузке?**  
    Через: `insmod module.ko param=value`.

23. **Зачем нужен `MODULE_LICENSE()`?**  
    Чтобы ядро разрешило использование GPL-only символов.

24. **Что произойдёт, если не указать лицензию?**  
    Модуль будет помечен как proprietary, доступ к многим API будет закрыт.

### Безопасность
25. **Какие основные правила безопасного кода в ядре?**  
    Проверка указателей, блокировок, ошибок системных функций.

26. **Можно ли использовать бесконечный цикл в модуле?**  
    Нет — он зависает ядро.

27. **Почему в ядре нет FPU операций?**  
    FPU требует сохранения контекста, что дорого и рискованно.

28. **Что делать, если модуль вызвал kernel panic?**  
    Перезагрузка, анализ dmesg, исправление ошибок.

### Практические вопросы
29. **Как узнать, какие модули загружены?**  
    Командой: `lsmod`.

30. **Как получить информацию о модуле?**  
    `modinfo module.ko`.
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
