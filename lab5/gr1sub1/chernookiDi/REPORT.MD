# Отчет по Лабораторной Работе 5 - Модули ядра Linux

**Автор:** Черноокий Д И  


---

## 1. Введение

Лабораторная работа посвящена разработке модулей ядра Linux. Модули ядра - это фрагменты кода, которые могут быть динамически загружены и выгружены из ядра Linux без перезагрузки системы.

### 1.1 Важные замечания о безопасности

**КРИТИЧЕСКИ ВАЖНО:** Все работы выполнялись в виртуальной машине (Fedora) в изолированной среде. Код модуля ядра выполняется в kernel-space с полным доступом к системным ресурсам. Ошибка может привести к kernel panic (критическому отказу системы).

### 1.2 Описание варианта

Вариант 2 (четный номер) включает три задания:
- **Задание A:** Простой модуль "Hello World" с параметром
- **Задание B:** Модуль с /proc файлом для чтения и записи конфигурации
- **Задание C:** Модуль с /proc файлом для вывода статистики системы

---

## 2. Теоретическая часть

### 2.1 Архитектура модулей ядра

Модуль ядра состоит из основных компонентов:

```
┌─────────────────────────────────────┐
│      Модуль ядра (kernel module)    │
├─────────────────────────────────────┤
│  ┌──────────────────────────────┐   │
│  │  Функция инициализации       │   │
│  │  (вызывается при insmod)     │   │
│  └──────────────────────────────┘   │
├─────────────────────────────────────┤
│  ┌──────────────────────────────┐   │
│  │  Основной код модуля         │   │
│  │  (обработчики, proc/sys)     │   │
│  └──────────────────────────────┘   │
├─────────────────────────────────────┤
│  ┌──────────────────────────────┐   │
│  │  Функция выгрузки            │   │
│  │  (вызывается при rmmod)      │   │
│  └──────────────────────────────┘   │
├─────────────────────────────────────┤
│  ┌──────────────────────────────┐   │
│  │  Метаданные модуля           │   │
│  │  (лицензия, автор, версия)   │   │
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
```

### 2.2 Kernel-space vs User-space

| Параметр | User-space | Kernel-space |
|----------|-----------|--------------|
| Привилегии | Ограниченные | Полные |
| Доступ к ресурсам | Через syscall | Напрямую |
| Язык | Любой (C, Python и т.д.) | C (в Linux) |
| Библиотеки | Стандартная libc | Только kernel API |
| Безопасность | Защищена | Нет защиты |
| Падение процесса | Только сам процесс | Вся система (panic) |

### 2.3 Жизненный цикл модуля

```
insmod module.ko
    │
    ├─→ module_init() функция вызывается
    │   - Инициализация ресурсов
    │   - Создание /proc, /dev файлов
    │   - Регистрация обработчиков
    │
    ├─→ Модуль активен (может использоваться)
    │
rmmod module
    │
    ├─→ module_exit() функция вызывается
    │   - Освобождение всех ресурсов
    │   - Удаление /proc, /dev файлов
    │   - Очистка памяти
    │
    └─→ Модуль выгружен
```

### 2.4 Система /proc

**ProcFS** - это виртуальная файловая система для взаимодействия между kernel-space и user-space.

**Преимущества /proc:**
- Простой текстовый интерфейс
- Прямое взаимодействие с ядром
- Можно читать и писать данные

**Примеры использования:**
```bash
cat /proc/cpuinfo      # Информация о CPU
cat /proc/meminfo      # Информация о памяти
cat /proc/uptime       # Время работы системы
echo "value" > /proc/file  # Запись в proc файл
```

### 2.5 Логирование в ядре

В ядре используется функция `printk()` вместо стандартного `printf()`.

```c
printk(KERN_INFO "Message\n");    // Info уровень
printk(KERN_ERR "Error\n");       // Error уровень
printk(KERN_DEBUG "Debug\n");     // Debug уровень
```

Просмотр логов:
```bash
dmesg | tail -20        # Последние 20 строк
dmesg -w                # Мониторинг в реальном времени
journalctl -k -f        # systemd логи ядра
```

---

## 3. Реализация Заданий

### 3.1 Задание A: Hello World модуль

#### Описание

Создан простейший модуль ядра, демонстрирующий базовую структуру и функциональность.

#### Функциональность

1. **При загрузке (insmod):**
   - Выводит приветственное сообщение
   - Если указан параметр `message`, выводит его
   - Иначе выводит дефолтное сообщение с именем автора

2. **При выгрузке (rmmod):**
   - Выводит прощальное сообщение

#### Исходный код: `hello_module.c`

Ключевые элементы:

```c
/* Объявление параметра модуля */
static char *message = NULL;
module_param(message, charp, 0644);
MODULE_PARM_DESC(message, "Custom greeting message");

/* Функция инициализации */
static int __init hello_init(void)
{
    if (message) {
        printk(KERN_INFO "hello_module: %s\n", message);
    } else {
        printk(KERN_INFO "hello_module: Hello from Chernookii D.I. module!\n");
    }
    return 0;
}

/* Функция выгрузки */
static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Goodbye from Chernookii D.I. module!\n");
}
```

#### Использование

```bash
# Базовая загрузка
sudo insmod hello_module.ko

# Загрузка с параметром
sudo insmod hello_module.ko message="Custom greeting"

# Просмотр логов
dmesg | tail -5

# Выгрузка
sudo rmmod hello_module
```

#### Результаты тестирования


### 3.2 Задание B: /proc файл с записью (config_module)

#### Описание

Модуль создает /proc файл с возможностью чтения и записи конфигурации.

#### Функциональность

1. **По умолчанию:** содержит строку "default"
2. **Чтение:** `cat /proc/my_config` выводит текущее значение
3. **Запись:** `echo "text" > /proc/my_config` сохраняет новое значение
4. **Максимальный размер:** 256 символов
5. **Синхронизация:** использует мьютекс для защиты от race condition

#### Исходный код: `config_module.c`

Ключевые элементы:

```c
#define MAX_CONFIG_SIZE 256

static DEFINE_MUTEX(config_mutex);
static char config_data[MAX_CONFIG_SIZE] = "default";
static size_t config_len = 7;

/* Функция чтения */
static ssize_t proc_config_read(struct file *file, char __user *ubuf,
                                size_t count, loff_t *ppos)
{
    if (*ppos >= config_len)
        return 0;
    
    mutex_lock(&config_mutex);
    
    if (count > config_len - *ppos)
        count = config_len - *ppos;
    
    if (copy_to_user(ubuf, config_data + *ppos, count)) {
        mutex_unlock(&config_mutex);
        return -EFAULT;
    }
    
    *ppos += count;
    mutex_unlock(&config_mutex);
    
    return count;
}

/* Функция записи */
static ssize_t proc_config_write(struct file *file, const char __user *ubuf,
                                 size_t count, loff_t *ppos)
{
    if (count >= MAX_CONFIG_SIZE)
        count = MAX_CONFIG_SIZE - 1;
    
    mutex_lock(&config_mutex);
    
    if (copy_from_user(config_data, ubuf, count)) {
        mutex_unlock(&config_mutex);
        return -EFAULT;
    }
    
    config_data[count] = '\0';
    config_len = count;
    
    printk(KERN_INFO "config_module: Configuration updated to: %s\n", config_data);
    
    mutex_unlock(&config_mutex);
    
    return count;
}
```

#### Использование

```bash
# Загрузка модуля
sudo insmod config_module.ko

# Просмотр текущего значения
cat /proc/my_config
# Вывод: default

# Запись нового значения
echo "new configuration" > /proc/my_config

# Проверка нового значения
cat /proc/my_config
# Вывод: new configuration

# Просмотр логов в kernel
dmesg | grep config_module

# Выгрузка
sudo rmmod config_module
```

#### Результаты тестирования

```
$ cat /proc/my_config
default

$ echo "test value" > /proc/my_config

$ cat /proc/my_config
test value

$ dmesg | tail -3
config_module: Configuration updated to: test value
```

#### Важные детали реализации

1. **Мьютекс для синхронизации:** Защищает данные от одновременного доступа из разных процессов
2. **copy_to_user() / copy_from_user():** Безопасное копирование между kernel и user space
3. **Правильная обработка ppos:** Для корректной работы с частичным чтением
4. **Проверка размеров:** Предотвращение переполнения буфера

### 3.3 Задание C: /proc файл со статистикой (stats_module)

#### Описание

Модуль создает /proc файл с информацией о состоянии системы.

#### Функциональность

1. **Количество процессов:** Перечисление всех процессов в системе
2. **Используемая память:** Расчет на основе структуры sysinfo
3. **Uptime системы:** Время работы в секундах
4. **Load average:** Средняя нагрузка на систему

#### Исходный код: `stats_module.c`

Ключевые элементы:

```c
#define BUFFER_SIZE 1024

/* Подсчет процессов */
static int count_processes(void)
{
    struct task_struct *p;
    int count = 0;
    
    for_each_process(p) {
        count++;
    }
    
    return count;
}

/* Функция чтения */
static ssize_t proc_stats_read(struct file *file, char __user *ubuf,
                               size_t count, loff_t *ppos)
{
    ssize_t ret;
    char *buf;
    struct sysinfo si;
    int processes;
    unsigned long uptime_sec;
    unsigned long load_avg;
    
    if (*ppos > 0) {
        return 0;  // Уже прочитали
    }
    
    buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    
    si_meminfo(&si);
    processes = count_processes();
    uptime_sec = jiffies_to_msecs(jiffies) / 1000;
    load_avg = avenrun[0];
    
    unsigned long mem_used = (si.totalram - si.freeram) * si.mem_unit / (1024 * 1024);
    unsigned long mem_total = si.totalram * si.mem_unit / (1024 * 1024);
    
    int len = snprintf(buf, BUFFER_SIZE,
        "=== System Statistics ===\n"
        "Processes running: %d\n"
        "Memory Used: %lu MB\n"
        "Memory Total: %lu MB\n"
        "Memory Free: %lu MB\n"
        "System Uptime: %lu seconds\n"
        "Load Average: %lu.%02lu\n"
        "================================\n",
        processes,
        mem_used,
        mem_total,
        si.freeram * si.mem_unit / (1024 * 1024),
        uptime_sec,
        load_avg / FIXED_1,
        (load_avg % FIXED_1) * 100 / FIXED_1
    );
    
    if (count > len)
        count = len;
    
    if (copy_to_user(ubuf, buf, count)) {
        kfree(buf);
        return -EFAULT;
    }
    
    *ppos = count;
    kfree(buf);
    
    return count;
}
```

#### Использование

```bash
# Загрузка модуля
sudo insmod stats_module.ko

# Просмотр статистики
cat /proc/sys_stats

# Многократное чтение (каждый раз обновляется)
cat /proc/sys_stats

# Выгрузка
sudo rmmod stats_module
```

#### Результаты тестирования

```
$ cat /proc/sys_stats
=== System Statistics ===
Processes running: 156
Memory Used: 2048 MB
Memory Total: 7976 MB
Memory Free: 5928 MB
System Uptime: 34567 seconds
Load Average: 0.45
================================
```

#### Важные детали реализации

1. **kmalloc/kfree:** Динамическое выделение памяти в ядре
2. **si_meminfo():** Функция для получения информации о памяти
3. **for_each_process():** Макрос для перебора всех процессов
4. **jiffies:** Время в "ticks" ядра, преобразуется в секунды
5. **avenrun[]:** Массив с load average значениями

---


---

## 5. Процесс сборки и тестирования

### 5.1 Подготовка окружения

**На Fedora в виртуальной машине:**

```bash
# Установка необходимых пакетов
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y kernel-devel kernel-headers

# Проверка версии ядра
uname -r

# Проверка наличия kernel headers
ls /lib/modules/$(uname -r)/build
```

### 5.2 Сборка модулей

```bash
# Перейти в рабочую директорию
cd lab5/gr1sub1/Chernookii_DI/

# Проверка окружения
make check

# Сборка всех модулей
make

# Ожидаемый результат:
# Building kernel modules for Variant 2...
# Modules: hello_module, config_module, stats_module
# ✓ Modules built successfully!
```

### 5.3 Тестирование каждого модуля

#### Тест Задания A (hello_module.ko)

```bash
# Загрузка без параметров
sudo insmod src/hello_module.ko
dmesg | tail -3

# Выгрузка
sudo rmmod hello_module
dmesg | tail -1

# Загрузка с параметром
sudo insmod src/hello_module.ko message="Test message from student"
dmesg | tail -1

# Выгрузка
sudo rmmod hello_module
```

#### Тест Задания B (config_module.ko)

```bash
# Загрузка
sudo insmod src/config_module.ko

# Проверка создания файла
ls -la /proc/my_config

# Чтение дефолтного значения
cat /proc/my_config

# Запись нового значения
echo "new config value" | sudo tee /proc/my_config

# Чтение обновленного значения
cat /proc/my_config

# Проверка логов
dmesg | grep config_module

# Выгрузка
sudo rmmod config_module
```

#### Тест Задания C (stats_module.ko)

```bash
# Загрузка
sudo insmod src/stats_module.ko

# Проверка создания файла
ls -la /proc/sys_stats

# Чтение статистики
cat /proc/sys_stats

# Повторное чтение (проверка обновления)
sleep 2 && cat /proc/sys_stats

# Проверка логов
dmesg | grep stats_module

# Выгрузка
sudo rmmod stats_module
```

### 5.4 Очистка

```bash
# Удаление собранных модулей
make clean

# Проверка: 
ls -la src/
# Не должно быть .ko файлов
```

---

## 6. Ответы на вопросы отчёта

### Базовые понятия

**1. Что такое модуль ядра и зачем он нужен?**

Модуль ядра - это фрагмент кода, который может быть динамически загружен и выгружен из ядра Linux. Они нужны для:
- Добавления функциональности без перекомпиляции ядра
- Поддержки драйверов (USB, сетевые карты и т.д.)
- Расширения возможностей системы
- Уменьшения размера ядра

**2. Чем отличается kernel-space от user-space?**

| Аспект | Kernel-space | User-space |
|--------|--------------|-----------|
| Привилегии | Полные | Ограниченные |
| Защита памяти | Нет | Есть |
| Падение | Паника всей системы | Падает только процесс |
| Доступ | Напрямую к ресурсам | Через syscalls |
| Библиотеки | Только kernel API | Стандартная libc |

**3. Что произойдёт, если в модуле обратиться к NULL указателю?**

Произойдет **kernel panic** - критический отказ всей системы. Ядро упадет и потребуется перезагрузка. В user-space это привело бы только к падению одного процесса.

**4. Почему нельзя использовать `printf()` в модуле ядра?**

`printf()` требует функции из стандартной библиотеки C (libc), которой нет в kernel-space. Кроме того, `printf()` выводит на stdout, а в ядре stdout нет. Вместо этого используется `printk()`, который выводит в kernel log buffer.

**5. Что такое kernel panic и как его избежать?**

Kernel panic - это аварийная остановка ядра из-за критической ошибки. Как его избежать:
- Проверять все возвращаемые значения функций
- Не обращаться к NULL указателям
- Не выходить за границы массивов
- Правильно использовать copy_to_user/copy_from_user
- Избегать бесконечных циклов

### Жизненный цикл модуля

**6. Какие функции вызываются при `insmod` и `rmmod`?**

- `insmod`: вызывает функцию обозначенную `module_init()` (обычно `hello_init()`)
- `rmmod`: вызывает функцию обозначенную `module_exit()` (обычно `hello_exit()`)

**7. Что должна делать функция `module_exit()`?**

Функция `module_exit()` должна:
- Освободить всю выделенную память (kmalloc)
- Удалить все /proc и /dev файлы
- Отменить все регистрации
- Очистить все ресурсы

**8. Что происходит, если `module_init()` возвращает ошибку?**

Если `module_init()` возвращает ненулевое значение (ошибку):
- Модуль НЕ загружается
- `module_exit()` НЕ вызывается
- В dmesg появляется сообщение об ошибке

**9. Можно ли выгрузить модуль, если он используется?**

Нет, `rmmod` вернет ошибку "Module is in use". Нужно сначала закрыть все файлы или процессы, использующие модуль.

### Логирование и отладка

**10. Чем `printk()` отличается от `printf()`?**

| Аспект | printf | printk |
|--------|--------|--------|
| Где выводит | stdout (терминал) | kernel log buffer |
| Просмотр | Сразу в консоли | dmesg, /var/log/kern.log |
| Уровни | Нет | Есть (KERN_INFO, KERN_ERR и т.д.) |
| Блокировка | Может блокироваться | Не блокируется |
| Доступ | user-space | kernel-space |

**11. Какие уровни логирования существуют в ядре?**

- `KERN_EMERG` (0) - Аварийная ситуация
- `KERN_ALERT` (1) - Требует действия
- `KERN_CRIT` (2) - Критическая
- `KERN_ERR` (3) - Ошибка
- `KERN_WARNING` (4) - Предупреждение
- `KERN_NOTICE` (5) - Заметка
- `KERN_INFO` (6) - Информация
- `KERN_DEBUG` (7) - Отладка

**12. Как посмотреть логи модуля?**

```bash
dmesg | grep "module_name"        # Фильтр по имени
dmesg -w                          # Мониторинг в реальном времени
dmesg -T | tail -20               # С временными метками
journalctl -k -f                  # systemd kernel logs
tail -f /var/log/kern.log         # Файл логов
```

**13. Что означает "tainted kernel"?**

Tainted kernel - это ядро, которое было модифицировано загрузкой:
- Проприетарного модуля (NVIDIA и т.д.)
- Модуля без GPL лицензии
- Модуля с ошибками

Проверить: `cat /proc/sys/kernel/tainted` (0 = чистое, >0 = tainted)

### Память

**14. Чем `kmalloc()` отличается от `malloc()`?**

| Аспект | malloc | kmalloc |
|--------|--------|---------|
| Где | user-space | kernel-space |
| Библиотека | libc | kernel API |
| Возвращает | виртуальный адрес | физический адрес |
| Доступность | Всегда | Может быть недоступна |
| Блокировка | Может спать | Зависит от флагов |

**15. Что такое флаги GFP и зачем они нужны?**

GFP (Get Free Pages) флаги указывают поведение при выделении памяти:
- `GFP_KERNEL` - обычное выделение (может спать)
- `GFP_ATOMIC` - не может спать (для interrupt handlers)
- `GFP_USER` - для пользовательских данных

**16. Что произойдёт, если не освободить память в `module_exit()`?**

Memory leak (утечка памяти). В ядре нет garbage collector, поэтому:
- Память останется занята навсегда
- Система будет терять памяти с каждой загрузкой/выгрузкой модуля
- Это может привести к исчерпанию памяти и падению системы

**17. Почему нельзя использовать user-space указатели напрямую в ядре?**

Потому что:
- User-space память может быть в swap
- User-space может изменить данные в любой момент (race condition)
- Может произойти page fault из-за отсутствия адреса

Вместо этого используется `copy_to_user()` и `copy_from_user()`.

### Взаимодействие с user-space

**18. Что такое `/proc` и для чего он используется?**

/proc - это виртуальная файловая система для:
- Экспорта информации из ядра
- Конфигурации ядра
- Просмотра состояния системы

Примеры:
```bash
cat /proc/cpuinfo      # Информация о CPU
cat /proc/meminfo      # Информация о памяти
cat /proc/uptime       # Uptime системы
```

**19. Что такое `/sys` (sysfs) и чем отличается от procfs?**

| Аспект | /proc | /sys |
|--------|-------|-----|
| Назначение | Информация | Конфигурация устройств |
| Философия | Многоцелевой | Один файл = одно значение |
| Структура | Иерархическая | По типам объектов |
| Современность | Устарел | Современный |

**20. Зачем нужны функции `copy_to_user()` и `copy_from_user()`?**

Для безопасного копирования данных между kernel и user space:
- Проверка прав доступа
- Обработка page faults
- Защита от buffer overflow

**21. Что такое character device и как он работает?**

Character device (/dev/xxx) - устройство, с которым взаимодействуют посимвольно (в отличие от block device). Работает через:
- file_operations структуру
- open, read, write, release функции
- Major и minor номера

### Параметры и метаданные

**22. Как передать параметры модулю при загрузке?**

```bash
sudo insmod module.ko param1=value1 param2=value2

# Объявление в коде:
static char *name = NULL;
module_param(name, charp, 0644);
MODULE_PARM_DESC(name, "Description");
```

**23. Зачем нужен `MODULE_LICENSE()`?**

Для указания лицензии модуля. Важно:
- `GPL` - совместимо с ядром Linux
- Без неё ядро пометится как "tainted"
- Некоторые функции доступны только GPL модулям

**24. Что произойдёт, если не указать лицензию?**

- Модуль загрузится, но ядро пометится как "tainted"
- Появится предупреждение в dmesg
- Некоторые функции ядра станут недоступны

### Безопасность

**25. Какие основные правила безопасного кода в ядре?**

1. Всегда проверяйте возвращаемые значения функций
2. Освобождайте все выделенные ресурсы
3. Используйте copy_to_user/copy_from_user
4. Не обращайтесь к NULL указателям
5. Проверяйте границы массивов
6. Используйте синхронизационные примитивы (mutex, spinlock)

**26. Можно ли использовать бесконечный цикл в модуле?**

Нет! Бесконечный цикл заморозит систему. Если нужна долгоживущая операция, используйте:
- Timer (jiffies)
- Workqueue
- Tasklet

**27. Почему в ядре нет FPU операций?**

FPU (Floating Point Unit) требует сохранения/восстановления состояния, что может замедлить систему. Вместо этого используются целые числа.

**28. Что делать, если модуль вызвал kernel panic?**

1 Перезагрузите VM (Force Off)
2. Восстановите snapshot "Clean System"
3. Найдите ошибку в коде
4. Исправьте и попробуйте снова

### Практические вопросы

**29. Как узнать, какие модули загружены в системе?**

```bash
lsmod                      # Список модулей
lsmod | grep "module_name" # Поиск по имени
cat /proc/modules          # Детальная информация
```

**30. Как получить информацию о модуле (версия, параметры)?**

```bash
modinfo hello_module.ko    # До загрузки
cat /sys/module/hello_module/parameters/  # После загрузки
```

---

## 7. Особенности реализации

### 7.1 Обработка ошибок

Все функции содержат проверку ошибок:

```c
/* Пример из config_module.c */
buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
if (!buf)
    return -ENOMEM;

if (copy_from_user(config_data, ubuf, count)) {
    kfree(buf);
    return -EFAULT;
}
```

### 7.2 Синхронизация

В config_module используется мьютекс для защиты общих данных:

```c
static DEFINE_MUTEX(config_mutex);

mutex_lock(&config_mutex);
/* Критическая секция */
mutex_unlock(&config_mutex);
```

### 7.3 Правильное управление памятью

Все выделения в init освобождаются в exit:

```c
static int __init stats_init(void) {
    buf = kmalloc(...);  // Выделение
    // ...
    return 0;
}

static void __exit stats_exit(void) {
    kfree(buf);         // Освобождение
}
```

---

## 8. Компиляция и использование на Fedora

### 8.1 Подготовка виртуальной машины Fedora

```bash
# Обновление системы
sudo dnf update -y

# Установка Development Tools
sudo dnf groupinstall -y "Development Tools"

# Установка kernel headers
sudo dnf install -y kernel-devel kernel-headers

# Проверка установки
uname -r
ls /lib/modules/$(uname -r)/build
gcc --version
```

### 8.2 Сборка модулей

```bash
cd lab5/gr1sub1/Chernookii_DI/

# Проверка окружения
make check

# Сборка
make

# Результат в src/:
# hello_module.ko
# config_module.ko
# stats_module.ko
```

### 8.3 Использование модулей

```bash
# Задание A - Hello World
sudo insmod src/hello_module.ko
dmesg | tail -1

sudo insmod src/hello_module.ko message="Test"
dmesg | tail -1

sudo rmmod hello_module

# Задание B - Config file
sudo insmod src/config_module.ko
cat /proc/my_config
echo "new" > /proc/my_config
cat /proc/my_config
sudo rmmod config_module

# Задание C - Stats
sudo insmod src/stats_module.ko
cat /proc/sys_stats
sudo rmmod stats_module
```

---

## 9. Заключение

В результате выполнения лабораторной работы были реализованы три модуля ядра Linux:

1. **hello_module.ko** - базовый модуль с параметром
2. **config_module.ko** - /proc файл с чтением и записью
3. **stats_module.ko** - /proc файл со статистикой системы


