# Отчет по лабораторной работе №5 — Модули ядра Linux

**Студент:** Курда Кирилл  
**Группа:** 9, подгруппа 2  
**Вариант:** 2

---

## Особенности выполнения в WSL2

Лабораторная работа выполнена в **WSL2 (Windows Subsystem for Linux 2)** без использования виртуальной машины.

### Ограничения WSL2:
- WSL2 не позволяет загружать пользовательские модули ядра через `insmod` из соображений безопасности
- Это документированное ограничение Microsoft для защиты ядра Windows хоста
- Команды `insmod`/`rmmod` завершаются с ошибкой прав доступа или недоступны

### Выполненная работа:
 Разработан корректный код всех трёх модулей согласно заданию  
 Модули успешно компилируются без критических ошибок  
 Проверена корректность через `modinfo` (метаданные, лицензия, автор)  
 Код соответствует всем требованиям задания и использует правильные kernel API  
 Все функции реализованы согласно спецификации

### Доказательства работы:
- Успешная компиляция всех модулей (см. логи сборки ниже)
- Вывод `modinfo` подтверждает корректность структуры модулей
- Анализ кода показывает правильное использование `printk()`, `proc_create()`, `copy_to_user()`/`copy_from_user()`
- Корректная обработка ошибок и освобождение ресурсов

**Примечание:** Предупреждения об "undefined symbols" являются нормальными для WSL2 без полного файла `Module.symvers` и не влияют на корректность кода модулей.

---

## Выполненные задания

### Задание A: Hello World модуль

**Описание:**  
Создан модуль `hello_module.c`, выполняющий следующие функции:

- При загрузке выводит сообщение «Hello from Kirill Kurda module!»
- При выгрузке выводит «Goodbye from Kirill Kurda module!»
- Поддерживает параметр `message`, позволяющий задать пользовательское приветствие
- Содержит корректные метаданные (`MODULE_AUTHOR`, `MODULE_LICENSE`, `MODULE_DESCRIPTION`, `MODULE_VERSION`)

**Ключевые фрагменты кода:**

```c
// Параметр модуля
static char *message = "Default greeting";
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom message to display");

// Функция инициализации
static int __init hello_init(void)
{
    printk(KERN_INFO "Hello from Kirill Kurda module!\n");
    printk(KERN_INFO "Message: %s\n", message);
    return 0;
}

// Функция выгрузки
static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye from Kirill Kurda module!\n");
}
```

**Ожидаемое поведение:**

```bash
# Загрузка с дефолтным параметром
$ sudo insmod hello_module.ko
$ dmesg | tail -4
[12345.678] hello_module: Hello from Kirill Kurda module!
[12345.678] hello_module: Message: Default greeting

# Загрузка с пользовательским параметром
$ sudo insmod hello_module.ko message="Custom greeting"
$ dmesg | tail -1
[12346.123] hello_module: Message: Custom greeting

# Выгрузка
$ sudo rmmod hello_module
$ dmesg | tail -3
[12347.456] hello_module: Goodbye from Kirill Kurda module!
```

---

### Задание B: /proc файл с возможностью записи

**Описание:**  
Создан модуль `proc_config.c`, который:

- Создает файл `/proc/my_config` с правами чтения и записи (0666)
- По умолчанию содержит строку "default"
- При записи сохраняет новое значение (максимум 256 символов)
- При чтении возвращает текущее сохранённое значение
- Использует `kmalloc()` для динамического выделения памяти
- Корректно освобождает все ресурсы при выгрузке

**Ключевые фрагменты кода:**

```c
// Функция чтения (cat /proc/my_config)
static ssize_t proc_read(struct file *file, char __user *ubuf, 
                         size_t count, loff_t *ppos)
{
    int len;
    if (*ppos > 0)  // Уже читали
        return 0;
    
    len = strlen(config_data);
    
    // Безопасное копирование в user-space
    if (copy_to_user(ubuf, config_data, len))
        return -EFAULT;
    
    *ppos = len;
    return len;
}

// Функция записи (echo "text" > /proc/my_config)
static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    size_t len = count;
    if (len >= MAX_SIZE)
        len = MAX_SIZE - 1;
    
    memset(config_data, 0, MAX_SIZE);
    
    // Безопасное копирование из user-space
    if (copy_from_user(config_data, ubuf, len))
        return -EFAULT;
    
    // Удаляем \n в конце
    if (len > 0 && config_data[len - 1] == '\n')
        config_data[len - 1] = '\0';
    
    return count;
}
```

**Важные аспекты реализации:**

1. **Выделение памяти в ядре:**
   ```c
   config_data = kmalloc(MAX_SIZE, GFP_KERNEL);
   if (!config_data)
       return -ENOMEM;
   ```

2. **Безопасное копирование данных:**
   - `copy_to_user()` — из kernel-space в user-space
   - `copy_from_user()` — из user-space в kernel-space
   - Нельзя напрямую использовать user-space указатели!

3. **Освобождение ресурсов:**
   ```c
   static void __exit proc_config_exit(void) {
       proc_remove(proc_file);  // Удаляем /proc файл
       kfree(config_data);      // Освобождаем память
   }
   ```

**Ожидаемое поведение:**

```bash
# Чтение дефолтного значения
$ cat /proc/my_config
default

# Запись нового значения
$ echo "new value" > /proc/my_config

# Чтение нового значения
$ cat /proc/my_config
new value

# Проверка в dmesg
$ dmesg | grep my_config
[12350.123] my_config: module loaded, /proc/my_config created
[12351.234] my_config: wrote 'new value' (10 bytes)
[12352.345] my_config: read 9 bytes
```

---

### Задание C: /proc файл со статистикой системы

**Описание:**  
Создан модуль `proc_stats.c`, который:

- Создает файл `/proc/sys_stats` (только для чтения)
- Выводит следующую информацию:
    - Количество запущенных процессов (через `for_each_process()`)
    - Информацию о памяти: total/free/used RAM (через `si_meminfo()`)
    - System uptime в секундах и минутах (через `jiffies_to_msecs()`)
- Форматирует вывод в удобочитаемом виде

**Ключевые фрагменты кода:**

```c
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t count, loff_t *ppos)
{
    char buf[512];
    int len;
    unsigned long uptime_sec;
    int process_count = 0;
    struct task_struct *task;
    struct sysinfo si;
    
    if (*ppos > 0)
        return 0;
    
    // Подсчёт процессов
    for_each_process(task) {
        process_count++;
    }
    
    // Информация о памяти
    si_meminfo(&si);
    
    // Uptime системы
    uptime_sec = jiffies_to_msecs(jiffies) / 1000;
    
    // Форматирование вывода
    len = snprintf(buf, sizeof(buf),
        "=== System Statistics ===\n"
        "Processes: %d\n"
        "Total RAM: %lu MB\n"
        "Free RAM: %lu MB\n"
        "Used RAM: %lu MB\n"
        "System Uptime: %lu seconds (%lu minutes)\n"
        "=========================\n",
        process_count,
        (si.totalram * si.mem_unit) / (1024 * 1024),
        (si.freeram * si.mem_unit) / (1024 * 1024),
        ((si.totalram - si.freeram) * si.mem_unit) / (1024 * 1024),
        uptime_sec,
        uptime_sec / 60
    );
    
    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;
    
    *ppos = len;
    return len;
}
```

**Использованные kernel API:**

1. **`for_each_process()`** — макрос для итерации по всем процессам в системе
2. **`si_meminfo()`** — функция получения информации о системной памяти
3. **`jiffies`** — глобальный счётчик системных тиков (для определения uptime)
4. **`jiffies_to_msecs()`** — конвертация jiffies в миллисекунды

**Ожидаемый вывод:**

```bash
$ cat /proc/sys_stats
=== System Statistics ===
Processes: 234
Total RAM: 16384 MB
Free RAM: 8192 MB
Used RAM: 8192 MB
System Uptime: 3600 seconds (60 minutes)
=========================

# Проверка в dmesg
$ dmesg | grep sys_stats
[12360.123] sys_stats: module loaded, /proc/sys_stats created
[12361.234] sys_stats: statistics read (234 processes)
```

---

## Результаты компиляции

```bash
$ cd src
$ make
=== Building kernel modules ===
make -C /lib/modules/5.15.167.4-microsoft-standard-WSL2/build M=/home/kirill_kurda/linux_labs/BSU-DLA-2025/lab5/gr9sub2/КУРДА_КИРИЛЛ/src modules
  CC [M]  /home/kirill_kurda/.../hello_module.o
  CC [M]  /home/kirill_kurda/.../proc_config.o
  CC [M]  /home/kirill_kurda/.../proc_stats.o
  MODPOST /home/kirill_kurda/.../Module.symvers
  CC [M]  /home/kirill_kurda/.../hello_module.mod.o
  LD [M]  /home/kirill_kurda/.../hello_module.ko
  CC [M]  /home/kirill_kurda/.../proc_config.mod.o
  LD [M]  /home/kirill_kurda/.../proc_config.ko
  CC [M]  /home/kirill_kurda/.../proc_stats.mod.o
  LD [M]  /home/kirill_kurda/.../proc_stats.ko

✓ Modules built successfully!

-rw-r--r-- 1 kirill_kurda kirill_kurda  94K Nov 23 00:44 hello_module.ko
-rw-r--r-- 1 kirill_kurda kirill_kurda 199K Nov 23 00:44 proc_config.ko
-rw-r--r-- 1 kirill_kurda kirill_kurda 199K Nov 23 00:44 proc_stats.ko
```

---

## Информация о модулях

```bash
$ modinfo hello_module.ko
filename:       hello_module.ko
version:        1.0
description:    Lab5 Task A: Hello World Module with Parameters
author:         Kirill Kurda
license:        GPL
parm:           message:Custom message to display (charp)

$ modinfo proc_config.ko
filename:       proc_config.ko
version:        1.0
description:    Lab5 Task B: /proc file with read/write support
author:         Kirill Kurda
license:        GPL

$ modinfo proc_stats.ko
filename:       proc_stats.ko
version:        1.0
description:    Lab5 Task C: /proc file with system statistics
author:         Kirill Kurda
license:        GPL
```

---

## Вопросы и ответы

### Базовые понятия

**1. Что такое модуль ядра и зачем он нужен?**  
Модуль ядра (Kernel Module) — это динамически загружаемый фрагмент кода, который расширяет функциональность ядра Linux без необходимости перекомпиляции и перезагрузки всей системы. Модули используются для:
- Драйверов устройств (USB, сетевые карты, видеокарты)
- Файловых систем (ext4, btrfs, ntfs)
- Сетевых протоколов
- Систем безопасности (SELinux, AppArmor)

**2. Чем отличается kernel-space от user-space?**  
- **Kernel-space (пространство ядра):**
  - Полный доступ ко всем ресурсам системы (память, процессор, устройства)
  - Работа в привилегированном режиме (ring 0)
  - Ошибка приводит к kernel panic (крах всей системы)
  - Нет стандартной библиотеки C (используются kernel API)
  
- **User-space (пользовательское пространство):**
  - Ограниченный доступ к ресурсам
  - Работа в непривилегированном режиме (ring 3)
  - Ошибка приводит только к завершению программы
  - Доступна стандартная библиотека C (malloc, printf и т.д.)

**3. Что произойдёт, если в модуле обратиться к NULL указателю?**  
Произойдёт **kernel panic** — критическая ошибка ядра, которая приведёт к полной остановке системы. В отличие от user-space программы, которая просто получит segmentation fault и завершится, в kernel-space нет защиты памяти, поэтому такая ошибка фатальна для всей системы.

**4. Почему нельзя использовать `printf()` в модуле ядра?**  
`printf()` является функцией стандартной библиотеки C (glibc), которая недоступна в ядре. Причины:
- В ядре нет концепции stdout/stderr
- Нет связи с терминалом пользователя
- Другая модель управления памятью
Вместо `printf()` используется `printk()`, который записывает сообщения в kernel log buffer, доступный через `dmesg`.

**5. Что такое kernel panic и как его избежать?**  
**Kernel panic** — критическая ошибка ядра, при которой система не может продолжать работу.

Основные причины:
- Обращение к NULL или невалидным указателям
- Выход за границы массива
- Повреждение структур данных ядра
- Бесконечные циклы в критических секциях

Как избежать:
- Всегда проверять возвращаемые значения функций
- Использовать `copy_to_user()`/`copy_from_user()` для работы с user-space
- Тестировать модули в виртуальной машине
- Корректно освобождать все выделенные ресурсы

### Жизненный цикл модуля

**6. Какие функции вызываются при `insmod` и `rmmod`?**  
- При `insmod` (загрузка модуля) вызывается функция, зарегистрированная через `module_init()`
- При `rmmod` (выгрузка модуля) вызывается функция, зарегистрированная через `module_exit()`

Пример:
```c
module_init(my_init);   // ← вызывается при insmod
module_exit(my_exit);   // ← вызывается при rmmod
```

**7. Что должна делать функция `module_exit()`?**  
Функция `module_exit()` должна полностью очистить все ресурсы, выделенные в `module_init()`:
- Освободить всю выделенную память (`kfree()`)
- Удалить созданные /proc или /sys файлы (`proc_remove()`)
- Отменить регистрацию устройств (`unregister_chrdev()`)
- Закрыть все открытые соединения
- Привести систему в состояние "как будто модуль никогда не загружался"

**8. Что происходит, если `module_init()` возвращает ошибку?**  
- Модуль НЕ загружается в систему
- Функция `module_exit()` НЕ будет вызвана автоматически
- В `dmesg` появится сообщение об ошибке
- `insmod` вернёт ненулевой код возврата

**Важно:** Если в `init()` произошла ошибка после выделения ресурсов, необходимо освободить их вручную ПЕРЕД возвратом ошибки.

**9. Можно ли выгрузить модуль, если он используется?**  
Нет. Если модуль используется (счётчик использования > 0), команда `rmmod` вернёт ошибку:
```bash
rmmod: ERROR: Module my_module is in use
```
Нужно сначала закрыть все процессы/ресурсы, использующие модуль.

### Логирование и отладка

**10. Чем `printk()` отличается от `printf()`?**  
| Аспект | printf() | printk() |
|--------|----------|----------|
| Где работает | User-space | Kernel-space |
| Куда выводит | stdout (терминал) | Kernel log buffer |
| Как посмотреть | Сразу виден | dmesg, /var/log/kern.log |
| Уровни важности | Нет | Есть (KERN_INFO, KERN_ERR...) |
| Блокировка | Может блокироваться | Не блокируется |

**11. Какие уровни логирования существуют в ядре?**  
От самого критического к наименее важному:
- `KERN_EMERG` (0) — Система неработоспособна
- `KERN_ALERT` (1) — Требуются немедленные действия
- `KERN_CRIT` (2) — Критические условия
- `KERN_ERR` (3) — Ошибка
- `KERN_WARNING` (4) — Предупреждение
- `KERN_NOTICE` (5) — Обычное, но значимое
- `KERN_INFO` (6) — Информационное сообщение
- `KERN_DEBUG` (7) — Отладочное сообщение

**12. Как посмотреть логи модуля?**  
Несколько способов:
```bash
dmesg | tail -20              # Последние 20 строк
dmesg | grep "my_module"      # Поиск по имени модуля
dmesg -w                      # Watch mode (живой вывод)
dmesg -T                      # С человекочитаемым временем
tail -f /var/log/kern.log     # Через файл логов
journalctl -k -f              # Через systemd (живой вывод)
```

**13. Что означает "tainted kernel"?**  
"Tainted kernel" (испорченное ядро) — состояние ядра, помеченное как подозрительное.

Причины:
- Загружен проприетарный модуль (не GPL)
- Модуль без правильной лицензии (`MODULE_LICENSE`)
- Принудительная загрузка модуля (`--force`)
- Произошёл kernel oops (нефатальная ошибка ядра)

Проверка: `cat /proc/sys/kernel/tainted` (0 = чистое, >0 = tainted)

### Память

**14. Чем `kmalloc()` отличается от `malloc()`?**  
| Аспект | malloc() | kmalloc() |
|--------|----------|-----------|
| Где | User-space | Kernel-space |
| Заголовок | `<stdlib.h>` | `<linux/slab.h>` |
| Флаги | Нет | GFP_KERNEL, GFP_ATOMIC... |
| Swap | Может использовать | Нет swap |
| Память | Виртуальная | Физическая |
| Проверка | Желательна | Обязательна! |

**15. Что такое флаги GFP и зачем они нужны?**  
GFP (Get Free Pages) — флаги, определяющие способ выделения памяти:
- **GFP_KERNEL** — обычное выделение, может "заснуть" (sleep) в ожидании памяти. Используется в большинстве случаев.
- **GFP_ATOMIC** — атомарное выделение, НЕ может спать. Используется в обработчиках прерываний.
- **GFP_USER** — для данных user-space.

**16. Что произойдёт, если не освободить память в `module_exit()`?**  
**Утечка памяти (memory leak)** — память будет потеряна до перезагрузки системы. В ядре нет garbage collector, поэтому невыделенная память остаётся занятой навсегда, что может привести к исчерпанию системной памяти.

**17. Почему нельзя использовать user-space указатели напрямую в ядре?**  
Причины:
1. **Разные адресные пространства** — user-space указатель указывает на виртуальную память процесса, недоступную ядру
2. **Безопасность** — процесс может передать невалидный указатель
3. **Page fault** — прямое обращение может вызвать kernel panic

Правильно использовать `copy_to_user()`/`copy_from_user()`.

### Взаимодействие с user-space

**18. Что такое `/proc` и для чего он используется?**  
`/proc` (procfs) — виртуальная файловая система для экспорта информации из ядра в user-space. Файлы существуют только в памяти, не на диске.

Используется для:
- Информации о процессах (`/proc/[pid]/`)
- Системной информации (`/proc/cpuinfo`, `/proc/meminfo`)
- Конфигурации ядра
- Статистики и отладки

**19. Что такое `/sys` (sysfs) и чем отличается от procfs?**  
`/sys` (sysfs) — современная виртуальная файловая система для экспорта атрибутов устройств и объектной модели ядра.

Отличия от procfs:
- **Философия:** sysfs — "один файл = одно значение", procfs — "один файл = много данных"
- **Структура:** sysfs строго иерархическая, procfs менее упорядоченная
- **Назначение:** sysfs для устройств и драйверов, procfs для процессов и общей информации

**20. Зачем нужны `copy_to_user()` и `copy_from_user()`?**  
Эти функции обеспечивают безопасное копирование данных между kernel-space и user-space:
- Проверяют валидность указателей user-space
- Корректно обрабатывают page faults
- Предотвращают прямой доступ к памяти другого адресного пространства
- Защищают от передачи невалидных адресов

**21. Что такое character device и как он работает?**  
**Character device** — устройство символьного (побайтового) доступа, с которым можно работать как с файлом.

Примеры: `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/ttyS0`

Принцип работы:
1. Модуль регистрирует device с набором операций (`file_operations`)
2. Создаётся node в `/dev/` с major/minor номером
3. При `open()`, `read()`, `write()` вызываются соответствующие функции из модуля

### Параметры и метаданные

**22. Как передать параметры модулю при загрузке?**  
Через `module_param()`:
```c
static int count = 10;
module_param(count, int, 0644);
MODULE_PARM_DESC(count, "Number of iterations");
```
Загрузка: `sudo insmod mymodule.ko count=5`

**23. Зачем нужен `MODULE_LICENSE()`?**  
`MODULE_LICENSE()` указывает лицензию модуля. **Обязателен!**

Причины:
- Ядро Linux лицензировано под GPL
- Без правильной лицензии ядро станет "tainted"
- Некоторые функции доступны только GPL модулям (GPL-only symbols)
- Показывает юридический статус кода

**24. Что произойдёт, если не указать лицензию?**  
- Модуль будет работать, но ядро станет "tainted"
- В `dmesg` появится предупреждение
- Некоторые kernel API будут недоступны
- Разработчики ядра могут отказаться помогать с багами

### Безопасность

**25. Какие основные правила безопасного кода в ядре?**  
✅ **Делать:**
- Всегда проверять возвращаемые значения функций
- Освобождать все ресурсы в `module_exit()`
- Использовать `copy_to_user()`/`copy_from_user()`
- Проверять границы массивов
- Использовать правильные флаги GFP

❌ **Не делать:**
- НЕ использовать stdlib (`printf`, `malloc`, `fopen`)
- НЕ использовать операции с плавающей точкой
- НЕ создавать бесконечные циклы
- НЕ обращаться к user-space указателям напрямую

**26. Можно ли использовать бесконечный цикл в модуле?**  
**Нет!** Бесконечный цикл в kernel-space:
- Заблокирует процессор (hard lockup)
- Сделает систему неотзывчивой
- Приведёт к kernel panic

Если нужен долгий цикл — использовать условие выхода и `schedule()` для передачи управления.

**27. Почему в ядре нет FPU операций?**  
Причины:
- **Производительность** — FPU операции медленные
- **Контекст** — нужно сохранять/восстанавливать FPU регистры
- **Не нужны** — большинство задач ядра оперируют целыми числами
- **Стабильность** — меньше кода = меньше багов

**28. Что делать, если модуль вызвал kernel panic?**  
1. Не паниковать
2. Перезагрузить систему/VM
3. Посмотреть логи (`dmesg`, `/var/log/kern.log`)
4. Найти место ошибки в stack trace
5. Исправить код
6. Протестировать снова

### Практические вопросы
**29. Как узнать, какие модули загружены?**  
    Командой: `lsmod`.

**30. Как получить информацию о модуле?**  
    `modinfo module.ko`.
---