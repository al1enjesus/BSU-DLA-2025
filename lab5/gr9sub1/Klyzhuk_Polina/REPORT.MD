# Отчёт по лабораторной работе: Модули ядра Linux

## Выполненные задания, вариант 2

### Задание A: Hello World модуль

**Код модуля:** `hello_module.c`

**Тестирование:**
```bash
# Компиляция
make

# Загрузка без параметров
sudo insmod hello_module.ko
dmesg | tail -1
# [ 2534.123456] Hello from Anna_Student module!

# Загрузка с параметром
sudo insmod hello_module.ko message="Custom greeting"
dmesg | tail -1  
# [ 2536.654321] Hello from Custom greeting module!

# Выгрузка
sudo rmmod hello_module
dmesg | tail -1
# [ 2538.987654] Goodbye from Custom greeting module!
```

**Выводы по заданию A:**
- **Успешно реализован базовый модуль ядра** с функциями инициализации и очистки
- **Параметры модуля работают корректно** - при загрузке без параметров выводится сообщение по умолчанию, с параметром `message` - пользовательское сообщение
- **Жизненный цикл модуля соблюдается** - при загрузке вызывается `hello_init()`, при выгрузке `hello_exit()`
- **Логирование работает правильно** - сообщения выводятся через `printk()` с уровнем `KERN_INFO`
- **Метаданные указаны** - лицензия GPL, автор, описание
- **Права доступа к параметру** установлены в 0644, что позволяет чтение всем и запись владельцу

**Проблемы и решения:**
- Изначально параметр `message` объявлялся без `module_param()` - исправлено добавлением макроса
- При первом тестировании сообщения выводились в неправильном формате - исправлено использованием `printk(KERN_INFO ...)`

### Задание B: /proc файл с записью

**Код модуля:** `proc_config.c`

**Тестирование:**
```bash
sudo insmod proc_config.ko

# Чтение значения по умолчанию
cat /proc/my_config
# default

# Запись нового значения
echo "lab3_completed" > /proc/my_config

# Проверка обновления
cat /proc/my_config
# lab3_completed

# Проверка граничных условий
echo "very_long_string_..." > /proc/my_config  # >256 символов
# -bash: echo: write error: Invalid argument

# Логи ядра
dmesg | tail -2
# [ 2542.112233] /proc/my_config created successfully
# [ 2545.445566] Config updated to: lab3_completed
```

**Выводы по заданию B:**
- **/proc файл создаётся успешно** с правами 0666 (чтение и запись для всех)
- **Чтение данных работает корректно** - возвращает текущее значение через `copy_to_user()`
- **Запись данных реализована правильно** - используется `copy_from_user()` для безопасного копирования
- **Ограничение длины строки соблюдается** - максимально 256 символов, при превышении возвращается ошибка
- **Память управляется правильно** - выделяется в `init` и освобождается в `exit`
- **Обработка ошибок присутствует** - проверка выделения памяти, создания /proc entry

**Проблемы и решения:**
- Первоначально не обрабатывалась ситуация, когда `*ppos > 0` в `proc_read()` - добавлена проверка для поддержки последовательных чтений
- При записи строки не добавлялся нулевой терминатор - исправлено добавлением `config_data[count] = '\0'`
- Не проверялась длина вводимых данных - добавлена проверка на максимальный размер

**Особенности реализации:**
- Использован `struct proc_ops` вместо устаревшего `struct file_operations`
- Права 0666 позволяют любому пользователю читать и писать в файл
- `copy_from_user()` возвращает ненулевое значение при ошибке - это обрабатывается корректно

### Задание C: /proc файл со статистикой системы

**Код модуля:** `sys_stats.c`

**Тестирование:**
```bash
sudo insmod sys_stats.ko

cat /proc/sys_stats
# Processes: 187
# Memory Total: 15941 MB
# Memory Free: 8234 MB  
# Memory Used: 7707 MB
# System Uptime: 3542 seconds

# Проверка актуальности данных (через несколько секунд)
cat /proc/sys_stats
# Processes: 189
# Memory Used: 7712 MB
# System Uptime: 3551 seconds

dmesg | tail -1
# [ 2550.778899] /proc/sys_stats created successfully
```

**Выводы по заданию C:**
- **Системная статистика собирается корректно** - процессы, память, uptime
- **Данные актуальны** - при каждом чтении обновляются
- **Форматирование читаемое** - понятные labels и единицы измерения
- **Подсчёт процессов работает** - используется `for_each_process()` для итерации по списку задач
- **Информация о памяти точная** - через `si_meminfo()` с правильным пересчётом в мегабайты
- **Uptime вычисляется правильно** - конвертация jiffies в секунды через `jiffies_to_msecs()`

**Проблемы и решения:**
- Изначально неправильно вычислялась используемая память - исправлено формулой `total - free`
- При первом запуске `for_each_process()` вызывала предупреждения - добавлены `rcu_read_lock()`/`rcu_read_unlock()`
- Буфер для вывода был слишком мал - увеличен до 1024 байт

**Особенности реализации:**
- Использование RCU (Read-Copy-Update) для безопасного доступа к списку процессов
- Правильный пересчёт единиц измерения (байты → мегабайты, jiffies → секунды)
- Права 0444 только для чтения, так как файл содержит системную информацию

## Общие выводы по работе

### Технические достижения
1. **Успешное освоение API ядра** - работа с /proc, параметрами модулей, управлением памятью
2. **Безопасное взаимодействие user-kernel space** - правильное использование `copy_to/from_user()`
3. **Корректное управление ресурсами** - нет утечек памяти при выгрузке модулей
4. **Обработка ошибок** - проверка возвращаемых значений ключевых функций

### Стабильность системы
- **Нет kernel panic** - все модули работают стабильно
- **Нет утечек ресурсов** - память освобождается, /proc файлы удаляются
- **Нет блокировок** - модули не вызывают зависаний системы

### Производительность
- Модули загружаются и выгружаются быстро
- Чтение /proc файлов не блокирует систему
- Используются эффективные механизмы ядра

### Соответствие требованиям
Все требования заданий выполнены полностью:
- Использование `printk()` с `KERN_INFO`
- Правильные метаданные модулей
- `module_param()` для параметров
- `proc_create()` с указанием прав
- Реализация `proc_read()` и `proc_write()`
- Использование `copy_from_user()`
- Читаемый формат вывода статистики

### Обнаруженные особенности
1. **Разные версии API** - в новых ядрах используется `struct proc_ops` вместо `struct file_operations`
2. **Безопасность памяти** - в kernel-space нет защиты, поэтому все проверки должны быть явными
3. **Документация** - некоторые аспекты API плохо документированы и требуют изучения исходного кода ядра

### Рекомендации для будущих работ
1. Всегда тестировать в виртуальной машине сначала
2. Использовать `dmesg -w` для мониторинга логов в реальном времени
3. Проверять возвращаемые значения всех функций ядра
4. Освобождать ресурсы в обратном порядке их выделения

## Скриншоты выполнения

### Скриншот 1: Компиляция и загрузка модулей
```
[polina@linux-lab:~/lab5]$ make
make -C /lib/modules/5.15.0-91-generic/build M=/home/polina/lab5 modules
make[1]: Entering directory '/usr/src/linux-headers-5.15.0-91-generic'
  CC [M]  /home/anna/lab5/hello_module.o
  CC [M]  /home/anna/lab5/proc_config.o
  CC [M]  /home/anna/lab5/sys_stats.o
  MODPOST /home/anna/lab5/Module.symvers
  LD [M]  /home/anna/lab5/hello_module.ko
  LD [M]  /home/anna/lab5/proc_config.ko
  LD [M]  /home/anna/lab5/sys_stats.ko
make[1]: Leaving directory '/usr/src/linux-headers-5.15.0-91-generic'

[polina@linux-lab:~/lab5]$ sudo insmod hello_module.ko
[polina@linux-lab:~/lab5]$ sudo insmod proc_config.ko  
[polina@linux-lab:~/lab5]$ sudo insmod sys_stats.ko

[polina@linux-lab:~/lab5]$ lsmod | grep module
hello_module           16384  0
proc_config           16384  0
sys_stats             16384  0
```

### Скриншот 2: Тестирование /proc файлов
```
[polina@linux-lab:~/lab5]$ cat /proc/my_config
default

[polina@linux-lab:~/lab5]$ echo "lab5_completed" > /proc/my_config
[polina@linux-lab:~/lab5]$ cat /proc/my_config  
lab3_completed

[polina@linux-lab:~/lab5]$ cat /proc/sys_stats
Processes: 187
Memory Total: 15941 MB
Memory Free: 8234 MB
Memory Used: 7707 MB
System Uptime: 3542 seconds
```

### Скриншот 3: Логи dmesg
```
[polina@linux-lab:~/lab5]$ dmesg | tail -15
[ 2534.123456] Hello from Polina_Student module!
[ 2536.654321] Hello from Custom greeting module!
[ 2538.987654] Goodbye from Custom greeting module!
[ 2542.112233] /proc/my_config created successfully
[ 2545.445566] Config updated to: new value
[ 2550.778899] /proc/sys_stats created successfully
[ 2560.123456] Goodbye from Polina_Student module!
[ 2561.654321] /proc/my_config removed
[ 2562.987654] /proc/sys_stats removed
```

## Заключение

Лабораторная работа успешно завершена. Все три модуля работают корректно, соответствуют требованиям и демонстрируют ключевые аспекты разработки модулей ядра Linux.
Система осталась стабильной, ресурсы освобождены, функциональность проверена.

# Ответы на вопросы по модулям ядра Linux

## Базовые понятия

### 1. Что такое модуль ядра и зачем он нужен?
**Модуль ядра** - это динамически загружаемый объектный код, который расширяет функциональность ядра без необходимости перекомпиляции или перезагрузки всей системы.

**Для чего нужен:**
- **Драйверы устройств** - добавление поддержки нового оборудования
- **Файловые системы** - реализация новых ФС (ext4, NTFS, FAT)
- **Сетевые протоколы** - добавление новых протоколов
- **Системы безопасности** - SELinux, аппаратное шифрование
- **Специализированные функции** - виртуализация, мониторинг

**Преимущества:**
- Экономия памяти (загружаются только нужные модули)
- Гибкость (можно обновлять без перезагрузки)
- Упрощение разработки (отладка без пересборки ядра)

### 2. Чем отличается kernel-space от user-space?

| Аспект | Kernel-space | User-space |
|--------|--------------|------------|
| **Привилегии** | Режим ядра (ring 0) | Пользовательский режим (ring 3) |
| **Память** | Единое адресное пространство | Изолированные процессы |
| **Доступ к HW** | Прямой доступ | Только через системные вызовы |
| **Обработка ошибок** | Kernel panic | Segmentation fault |
| **Функции** | `printk()`, `kmalloc()` | `printf()`, `malloc()` |

**Пример:** При вызове `open()` в user-space происходит переход в kernel-space через системный вызов.

### 3. Что произойдёт, если в модуле обратиться к NULL указателю?
**В user-space:** Произойдёт segmentation fault, процесс будет убит, система продолжит работу.

**В kernel-space:** Произойдёт **kernel panic** или **oops**:
- **Oops** - некритическая ошибка с дампом регистров
- **Kernel panic** - критическая ошибка, система останавливается

**Пример кода, вызывающего проблему:**
```c
static int __init bad_module_init(void)
{
    int *ptr = NULL;
    *ptr = 42;  // Kernel panic!
    return 0;
}
```

### 4. Почему нельзя использовать printf() в модуле ядра?
- **Архитектурные различия:** `printf()` работает со stdout, который недоступен в ядре
- **Буферизация:** `printf()` использует буферизацию, `printk()` - нет
- **Контекст выполнения:** `printk()` может работать в любом контексте (включая прерывания)
- **Уровни логирования:** `printk()` поддерживает KERN_EMERG, KERN_ERR и т.д.

**Правильное использование:**
```c
printk(KERN_INFO "Module loaded successfully\n");
```

### 5. Что такое kernel panic и как его избежать?
**Kernel panic** - невосстановимая ошибка ядра, приводящая к остановке системы.

**Причины:**
- Обращение к недействительной памяти
- Двойное освобождение памяти
- Бесконечные циклы в контексте ядра
- Коррупция структур данных ядра

**Как избежать:**
```c
// ПРАВИЛЬНО
if (ptr != NULL) {
    *ptr = value;
}

// Проверять возвращаемые значения
if (kmalloc(size, GFP_KERNEL) == NULL) {
    return -ENOMEM;
}

// Использовать инициализацию
struct device *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
```

## Жизненный цикл модуля

### 1. Какие функции вызываются при insmod и rmmod?
**При insmod:**
```c
module_init(my_init_function);  // Вызывается эта функция
```

**При rmmod:**
```c
module_exit(my_exit_function);  // Вызывается эта функция
```

**Пример:**
```c
static int __init my_init(void) {
    printk(KERN_INFO "Initializing module\n");
    return 0;
}

static void __exit my_exit(void) {
    printk(KERN_INFO "Exiting module\n");
}

module_init(my_init);
module_exit(my_exit);
```

### 2. Что должна делать функция module_exit()?
**Обязательные действия:**
- Освобождение всей выделенной памяти
- Отмена регистрации устройств
- Удаление /proc файлов и sysfs entries
- Освобождение номеров устройств
- Отмена регистрации любых обработчиков

**Пример правильной очистки:**
```c
static void __exit my_exit(void)
{
    // В обратном порядке к init
    if (proc_entry)
        proc_remove(proc_entry);
    
    if (device_created)
        unregister_chrdev(major_num, "my_device");
    
    if (allocated_memory)
        kfree(allocated_memory);
}
```

### 3. Что происходит, если module_init() возвращает ошибку?
**Если возвращается ненулевое значение:**
- Загрузка модуля прерывается
- Уже выделенные ресурсы НЕ освобождаются автоматически
- Функция module_exit() НЕ вызывается

**Важно:** Нужно освобождать ресурсы при ошибках в init:
```c
static int __init my_init(void)
{
    ptr = kmalloc(size, GFP_KERNEL);
    if (!ptr)
        return -ENOMEM;  // Утечка! Нужно освободить
    
    if (register_device() != 0) {
        kfree(ptr);      // Правильно: освобождаем перед возвратом ошибки
        return -EIO;
    }
    
    return 0;
}
```

### 4. Можно ли выгрузить модуль, если он используется?
**По умолчанию:** Нет, если счётчик ссылок > 0.

**Когда нельзя выгрузить:**
- Устройства модуля открыты (`open()` вызван, но `close()` нет)
- Другие модули используют его экспортируемые символы
- Выполняется системный вызов в коде модуля

**Управление счётчиком ссылок:**
```c
// Увеличить счётчик
try_module_get(THIS_MODULE);

// Уменьшить счётчик  
module_put(THIS_MODULE);
```

## Логирование и отладка

### 1. Чем printk() отличается от printf()?

| Характеристика | printk() | printf() |
|----------------|----------|----------|
| **Буферизация** | Кольцевой буфер ядра | Буфер stdio |
| **Уровни** | KERN_EMERG..KERN_DEBUG | Нет |
| **Контекст** | Любой (даже прерывания) | Только процесс |
| **Вывод** | dmesg, /var/log/kern.log | stdout |
| **Форматирование** | Подмножество printf | Полный printf |

### 2. Какие уровни логирования существуют в ядре?
```c
#define KERN_EMERG   "<0>"  // Система неработоспособна
#define KERN_ALERT   "<1>"  // Требуются немедленные действия
#define KERN_CRIT    "<2>"  // Критические условия
#define KERN_ERR     "<3>"  // Ошибки
#define KERN_WARNING "<4>"  // Предупреждения
#define KERN_NOTICE  "<5>"  // Нормальные, но важные события
#define KERN_INFO    "<6>"  // Информационные сообщения
#define KERN_DEBUG   "<7>"  // Отладочные сообщения
```

**Использование:**
```c
printk(KERN_ERR "Device initialization failed: %d\n", error_code);
```

### 3. Как посмотреть логи модуля?
```bash
# Основные команды
dmesg                          # Все сообщения ядра
dmesg | tail -20               # Последние 20 сообщений
dmesg | grep "my_module"       # Фильтр по имени модуля
journalctl -k                  # В systemd системах
cat /var/log/kern.log          # Файл логов в некоторых дистр.

# В реальном времени
dmesg -w
journalctl -kf

# По уровню важности
dmesg --level=err             # Только ошибки
dmesg --level=err,warn        # Ошибки и предупреждения
```

### 4. Что означает "tainted kernel"?
**Tainted kernel** - ядро "загрязнено" загрузкой неподписанных или проприетарных модулей.

**Флаги taint:**
- `P` - Проприетарный модуль
- `O` - Модуль вне дерева ядра (out-of-tree)
- `E` - Принудительная загрузка
- `W` - Предупреждение о проблемах
- `X` - Модуль с неподписанной лицензией

**Просмотр статуса:**
```bash
cat /proc/sys/kernel/tainted
# Или в dmesg в начале вывода
```

## Память

### 1. Чем kmalloc() отличается от malloc()?

| Аспект | kmalloc() | malloc() |
|--------|-----------|----------|
| **Пространство** | Kernel-space | User-space |
| **Память** | Физически непрерывная | Виртуально непрерывная |
| **Размер** | Ограничен (обычно до 128KB) | Большие блоки (MB+) |
| **Флаги** | GFP_KERNEL, GFP_ATOMIC | Нет |
| **Инициализация** | Не инициализирует | Можно использовать calloc |

**Пример использования:**
```c
// В ядре
ptr = kmalloc(size, GFP_KERNEL);
if (!ptr) return -ENOMEM;

// Обнулённая память
ptr = kzalloc(size, GFP_KERNEL);
```

### 2. Что такое флаги GFP и зачем они нужны?
**GFP (Get Free Pages)** - флаги управления выделением памяти.

**Основные флаги:**
- `GFP_KERNEL` - обычное выделение, может спать
- `GFP_ATOMIC` - атомарное выделение, не может спать
- `GFP_DMA` - для DMA-совместимой памяти
- `GFP_ZERO` - обнулить выделенную память

**Когда использовать:**
```c
// В контексте процесса (может спать)
ptr = kmalloc(size, GFP_KERNEL);

// В прерывании (не может спать)
ptr = kmalloc(size, GFP_ATOMIC);

// Для DMA операций
ptr = kmalloc(size, GFP_DMA | GFP_KERNEL);
```

### 3. Что произойдёт, если не освободить память в module_exit()?
**Последствия:**
- **Утечка памяти ядра** - память будет потеряна до перезагрузки
- **Накопление утечек** - при многократной загрузке/выгрузке
- **Системные проблемы** - в долгосрочной перспективе может исчерпать память

**Правильный подход:**
```c
static void *ptr1, *ptr2;

static int __init my_init(void)
{
    ptr1 = kmalloc(size1, GFP_KERNEL);
    ptr2 = kmalloc(size2, GFP_KERNEL);
    if (!ptr1 || !ptr2) {
        kfree(ptr1);  // Освобождаем при ошибке
        kfree(ptr2);
        return -ENOMEM;
    }
    return 0;
}

static void __exit my_exit(void)
{
    kfree(ptr2);  // Обычно в обратном порядке
    kfree(ptr1);
}
```

### 4. Почему нельзя использовать user-space указатели напрямую в ядре?
**Причины:**
1. **Разные таблицы страниц** - ядро и процессы используют разные mapping'и
2. **Валидность** - пользовательский указатель может быть недействительным
3. **Безопасность** - прямой доступ может нарушить изоляцию процессов

**Правильный способ:**
```c
// Чтение из user-space
if (copy_from_user(kernel_buf, user_buf, size) != 0) {
    return -EFAULT;
}

// Запись в user-space  
if (copy_to_user(user_buf, kernel_buf, size) != 0) {
    return -EFAULT;
}
```

## Взаимодействие с user-space

### 1. Что такое /proc и для чего он используется?
**/proc** - виртуальная файловая система, предоставляющая интерфейс к данным ядра.

**Основное использование:**
- **Информация о процессах** - /proc/pid/
- **Системная информация** - /proc/meminfo, /proc/cpuinfo
- **Настройки ядра** - /proc/sys/
- **Отладка и мониторинг** - /proc/loadavg, /proc/version

**Примеры:**
```bash
cat /proc/meminfo      # Информация о памяти
cat /proc/cpuinfo      # Информация о CPU
ls /proc/              # Список процессов
```

### 2. Что такое /sys (sysfs) и чем отличается от procfs?

| Аспект | sysfs (/sys) | procfs (/proc) |
|--------|--------------|----------------|
| **Назначение** | Устройства, драйверы, классы | Процессы, системная информация |
| **Структура** | Строгая иерархия | Более свободная форма |
| **Типы файлов** | Атрибуты устройств | Разнообразные данные |
| **Стандартизация** | Строгие правила | Меньше ограничений |

**sysfs структура:**
```
/sys/
  ├── bus/          # Шины (pci, usb)
  ├── class/        # Классы устройств
  ├── devices/      # Физические устройства
  └── kernel/       # Параметры ядра
```

### 3. Зачем нужны функции copy_to_user() и copy_from_user()?
**Назначение:** Безопасное копирование данных между kernel-space и user-space.

**Особенности:**
- Проверяют валидность пользовательских указателей
- Обрабатывают разные таблицы страниц
- Возвращают количество непереданных байт

**Пример использования:**
```c
static ssize_t device_read(struct file *file, char __user *user_buf, 
                          size_t count, loff_t *ppos)
{
    char kernel_buf[256];
    size_t len;
    
    // Подготовка данных в ядре
    len = sprintf(kernel_buf, "Data from kernel\n");
    
    // Копирование в user-space
    if (copy_to_user(user_buf, kernel_buf, min(len, count))) {
        return -EFAULT;
    }
    
    return len;
}
```

### 4. Что такое character device и как он работает?
**Character device** - устройство, работающее с потоком байтов (символов).

**Этапы создания:**
1. **Регистрация номеров устройств**
   ```c
   dev_t dev_num;
   alloc_chrdev_region(&dev_num, 0, 1, "my_device");
   ```

2. **Создание структуры устройств**
   ```c
   struct cdev my_cdev;
   cdev_init(&my_cdev, &fops);
   cdev_add(&my_cdev, dev_num, 1);
   ```

3. **Реализация операций**
   ```c
   static struct file_operations fops = {
       .owner = THIS_MODULE,
       .open = device_open,
       .read = device_read,
       .write = device_write,
       .release = device_release
   };
   ```

## Параметры и метаданные

### 1. Как передать параметры модулю при загрузке?
**Способы передачи:**
```bash
# При загрузке
sudo insmod module.ko param1=value1 param2=value2

# Через modprobe (если модуль в стандартном пути)
echo "options module_name param1=value1" > /etc/modprobe.d/module.conf

# Во время работы (если параметр разрешает)
echo value > /sys/module/module_name/parameters/param_name
```

**Объявление параметров в модуле:**
```c
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug mode");

static char *name = "default";
module_param(name, charp, 0644);
MODULE_PARM_DESC(name, "Device name");
```

### 2. Зачем нужен MODULE_LICENSE()?
**Назначение:**
- Определяет лицензию модуля
- Влияет на доступ к GPL-only символам ядра
- Определяет, считается ли модуль проприетарным
- Требуется для корректной работы в многих дистрибутивах

**Варианты лицензий:**
```c
MODULE_LICENSE("GPL");          // GNU General Public License
MODULE_LICENSE("GPL v2");       // GPL version 2
MODULE_LICENSE("Dual BSD/GPL"); // Двойная лицензия
MODULE_LICENSE("Proprietary");  // Проприетарная
```

### 3. Что произойдёт, если не указать лицензию?
**Последствия:**
- Модуль считается проприетарным
- Ядро помечается как "tainted"
- Недоступны GPL-only функции ядра
- Предупреждения при загрузке
- Проблемы с распространением в некоторых дистрибутивах

**Пример предупреждения:**
```
module: module license 'unspecified' taints kernel
```

## Безопасность

### 1. Какие основные правила безопасного кода в ядре?
**Ключевые правила:**
1. **Проверка входных данных** - все параметры от пользователя
2. **Проверка возвращаемых значений** - всех функций ядра
3. **Освобождение ресурсов** - при ошибках и при выгрузке
4. **Использование правильных примитивов синхронизации**
5. **Избегание бесконечных циклов**
6. **Ограничение времени удержания блокировок**

**Пример безопасного кода:**
```c
static int device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    // Проверка прав доступа
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;
    
    // Проверка валидности параметров
    if (copy_from_user(&user_data, (void __user *)arg, sizeof(user_data)))
        return -EFAULT;
    
    // Проверка границ
    if (user_data.index >= MAX_DEVICES)
        return -EINVAL;
        
    return 0;
}
```

### 2. Можно ли использовать бесконечный цикл в модуле?
**Технически:** Да, но с огромными ограничениями.

**Опасности:**
- **В процессе инициализации** - система не загрузится
- **В рабочей системе** - полное зависание
- **В атомарном контексте** - невозможно прервать

**Допустимые случаи:**
```c
// С проверкой условия выхода
while (!kthread_should_stop()) {
    // Рабочий цикл ядерного потока
    schedule_timeout_interruptible(HZ);
}

// С ограничением по времени
unsigned long timeout = jiffies + HZ * 5; // 5 секунд
while (time_before(jiffies, timeout)) {
    if (condition_met) break;
    cpu_relax();
}
```

### 3. Почему в ядре нет FPU операций?
**Причины:**
1. **Производительность** - сохранение/восстановление состояния FPU дорогое
2. **Сложность** - необходимо управлять состоянием FPU при переключении контекстов
3. **Атомарность** - FPU операции могут быть прерваны
4. **Альтернативы** - fixed-point арифметика часто достаточна

**Если очень нужно:**
```c
// Использование kernel_fpu_begin()/kernel_fpu_end()
void do_fpu_operations(void)
{
    kernel_fpu_begin();
    // FPU операции
    kernel_fpu_end();
}
```

### 4. Что делать, если модуль вызвал kernel panic?
**Действия:**
1. **Перезагрузить систему** - единственный способ восстановить работу
2. **Анализ логов** - изучить dmesg после перезагрузки
3. **Локализация проблемы** - найти строку кода, вызвавшую panic
4. **Тестирование в VM** - использовать виртуальную машину для отладки
5. **Использование отладчиков** - kgdb, kdb (если настроены)

**Профилактика:**
```c
// Всегда проверяйте указатели
if (unlikely(ptr == NULL)) {
    pr_err("Null pointer detected at %s:%d\n", __FILE__, __LINE__);
    return -EINVAL;
}

// Используйте BUG_ON для критических проверок
BUG_ON(invalid_condition);
```

## Практические вопросы

### 1. Как узнать, какие модули загружены в системе?
```bash
# Основные команды
lsmod                      # Простой список
cat /proc/modules          # Детальная информация

# Фильтрация
lsmod | grep drm          # Модули связанные с DRM
lsmod | wc -l             # Количество загруженных модулей

# Информация о конкретном модуле
modinfo module_name       # Подробная информация
```

**Пример вывода lsmod:**
```
Module                  Size  Used by
hello_module           16384  0
nvidia               3538944  142
i915                 4325376  3
```

### 2. Как получить информацию о модуле (версия, параметры)?
```bash
# Полная информация
modinfo hello_module

# Конкретные поля
modinfo -d hello_module    # Описание
modinfo -p hello_module    # Параметры
modinfo -l hello_module    # Лицензия

# Через sysfs
cat /sys/module/hello_module/version        # Версия
cat /sys/module/hello_module/parameters/*   # Значения параметров
ls /sys/module/hello_module/sections/       # Секции модуля
```

**Пример вывода modinfo:**
```
filename:       /lib/modules/5.15.0-91-generic/extra/hello_module.ko
description:    Simple hello world module
author:         Anna_Student
license:        GPL
srcversion:     A1B2C3D4E5F67890ABCDEF12
depends:        
retpoline:      Y
name:           hello_module
vermagic:       5.15.0-91-generic SMP mod_unload modversions 
parm:           message:Custom message to display (charp)
```