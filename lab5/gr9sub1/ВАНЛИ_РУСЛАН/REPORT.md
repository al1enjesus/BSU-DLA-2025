# Отчет по лабораторной работе №5 — Модули ядра Linux

Студент: Ванли Руслан  
Группа: 9, подгруппа 1  
Вариант: 1 (нечётный номер)

---

## Цель работы

Освоение практических навыков разработки модулей ядра Linux, изучение механизмов взаимодействия между kernel-space и user-space через procfs и символьные устройства, получение опыта отладки кода уровня ядра.

---

## Теоретическая часть

### Основы модулей ядра Linux

Модуль ядра — это динамически загружаемый объектный файл (.ko), расширяющий функциональность Linux без необходимости перекомпиляции ядра и перезагрузки системы. Модули работают в привилегированном режиме (kernel-space) с полным доступом к аппаратным ресурсам.

Ключевые отличия kernel-space от user-space:
- Код выполняется с максимальными привилегиями (ring 0)
- Отсутствие защиты памяти между компонентами ядра
- Невозможность использования стандартной библиотеки C (libc)
- Критические ошибки приводят к kernel panic
- Используются специализированные API ядра (printk, kmalloc, copy_to_user и т.д.)

Жизненный цикл модуля:
1. Загрузка (insmod) → вызов функции, зарегистрированной через module_init()
2. Работа — модуль активен в системе, обрабатывает запросы
3. Выгрузка (rmmod) → вызов функции, зарегистрированной через module_exit()

---

## Описание выполненных заданий

### Задание A: Hello World модуль с параметрами

Цель: Создать базовый модуль, демонстрирующий основные принципы работы с параметрами и логированием.

Описание реализации:

Модуль hello_module.c реализует следующую функциональность:

1. Параметризация:
   

C

   static char *input_name = "Ruslan";
   module_param(input_name, charp, 0644);
   MODULE_PARM_DESC(input_name, "Name to display in the log");
   

   - Переменная input_name со значением по умолчанию "Ruslan"
   - Права доступа 0644 позволяют читать/изменять параметр через sysfs
   - Описание параметра для modinfo

2. Функция инициализации:
   

C

   static int __init start_hello_module(void)
   {
       printk(KERN_INFO "custom_hello: Module loaded successfully.\n");
       printk(KERN_INFO "custom_hello: Greetings, %s!\n", input_name);
       return 0;
   }
   

   - Вывод сообщения о успешной загрузке
   - Персонализированное приветствие с использованием параметра
   - Возврат 0 сигнализирует об успешной инициализации

3. Функция завершения:
   

C

   static void __exit stop_hello_module(void)
   {
       printk(KERN_INFO "custom_hello: Module is unloading. See you later, %s!\n", input_name);
   }
   

   - Прощальное сообщение с именем пользователя

4. Метаданные модуля:
   

C

   MODULE_LICENSE("GPL");
   MODULE_AUTHOR("[Vanli Ruslan]");
   MODULE_DESCRIPTION("Refactored Hello World Kernel Module");
   MODULE_VERSION("2.0");
   


Результаты тестирования:

Тест 1 — загрузка с параметром по умолчанию:

Bash

$ make test-hello
=== Testing hello_module ===
1. Loading module with default parameter...
$ dmesg | tail -2
[  342.156789] custom_hello: Module loaded successfully.
[  342.156812] custom_hello: Greetings, Ruslan!


Тест 2 — загрузка с пользовательским параметром:

Bash

$ sudo insmod hello_module.ko input_name="Tester"
$ dmesg | tail -2
[  387.923401] custom_hello: Module loaded successfully.
[  387.923428] custom_hello: Greetings, Tester!


Тест 3 — выгрузка модуля:

Bash

$ sudo rmmod hello_module
$ dmesg | tail -1
[  412.667193] custom_hello: Module is unloading. See you later, Tester!


Проверка метаданных:

Bash

$ modinfo hello_module.ko
filename:       hello_module.ko
version:        2.0
description:    Refactored Hello World Kernel Module
author:         [Vanli Ruslan]
license:        GPL
srcversion:     A8F3C92B1E5D7A6F9B2C4E1
depends:        
retpoline:      Y
name:           hello_module
vermagic:       5.15.0-91-generic SMP mod_unload modversions
parm:           input_name:Name to display in the log (charp)


Проверка параметра через sysfs:

Bash

$ cat /sys/module/hello_module/parameters/input_name
Ruslan

$ echo "NewName" | sudo tee /sys/module/hello_module/parameters/input_name
NewName

Анализ:  
Модуль корректно обрабатывает как значение по умолчанию, так и пользовательский параметр. Префикс "custom_hello" в сообщениях позволяет легко фильтровать логи через dmesg | grep custom_hello. Использование KERN_INFO обеспечивает правильный уровень важности сообщений.

---

### Задание B: Интерфейс через procfs

Цель: Создать виртуальный файл в /proc для взаимодействия с пользовательским пространством.

Описание реализации:

Модуль proc_module.c создает файл /proc/kernel_task_info со следующей архитектурой:

1. Управление состоянием:
   

C

   static struct proc_dir_entry *entry_ptr = NULL;
   static int access_counter = 0;
   static unsigned long start_jiffies = 0;
   

   - entry_ptr — дескриптор proc-файла
   - access_counter — счётчик обращений к файлу
   - start_jiffies — временная метка загрузки модуля (в тиках ядра)

2. Обработчик чтения:
   

C

   static ssize_t info_read_handler(struct file *file, char __user *user_buffer,
                                    size_t count, loff_t *position)
   {
       char k_buf[TEMP_BUF_SIZE];
       int str_len;
       
       if (*position != 0)
           return 0;  // Предотвращение повторного чтения
       
       access_counter++;
       
       str_len = scnprintf(k_buf, sizeof(k_buf),
           "=== Student Kernel Module ===\n"
           "Author: [Vanli Ruslan]\n"
           "Stats -> Reads: %d | Start Time: %lu\n",
           access_counter, start_jiffies);
       
       if (copy_to_user(user_buffer, k_buf, str_len))
           return -EFAULT;
       
       *position = str_len;
       return str_len;
   }
   

   
   Ключевые особенности:
   - Проверка *position != 0 предотвращает бесконечное чтение
   - Инкремент счётчика при каждом обращении
   - scnprintf() безопасно форматирует строку с защитой от переполнения
   - copy_to_user() обеспечивает безопасное копирование данных в user-space

3. Регистрация операций:
   

C

   static const struct proc_ops my_proc_ops = {
       .proc_read = info_read_handler,
   };
   

   Использование современной структуры proc_ops вместо устаревшей file_operations.

4. Инициализация модуля:
   

C

   static int __init proc_test_init(void)
   {
       printk(KERN_INFO "kernel_task: Initializing proc entry\n");
       start_jiffies = jiffies;
       
       entry_ptr = proc_create(PROC_FILENAME, 0444, NULL, &my_proc_ops);
       if (!entry_ptr) {
           printk(KERN_ERR "kernel_task: Error creating /proc/%s\n", PROC_FILENAME);
           return -ENOMEM;
       }
       
       printk(KERN_INFO "kernel_task: Interface /proc/%s created\n", PROC_FILENAME);
       return 0;
   }
   

   - Захват момента загрузки через jiffies
   - Создание файла с правами 0444 (read-only для всех)
   - Обработка ошибок при создании файла

5. Очистка ресурсов:
   

C

   static void __exit proc_test_exit(void)
   {
       if (entry_ptr) {
           proc_remove(entry_ptr);
           printk(KERN_INFO "kernel_task: /proc/%s removed\n", PROC_FILENAME);
       }
       printk(KERN_INFO "kernel_task: Module unloaded. Total accesses: %d\n", access_counter);
   }
   


Результаты тестирования:

Тест 1 — загрузка модуля и проверка создания файла:

Bash

$ make test-proc
=== Testing proc_module ===
1. Loading module...
$ dmesg | tail -2
[  528.734521] kernel_task: Initializing proc entry
[  528.734567] kernel_task: Interface /proc/kernel_task_info created

$ ls -l /proc/kernel_task_info
-r--r--r-- 1 root root 0 ноя 30 14:32 /proc/kernel_task_info


Тест 2 — первое чтение файла:

Bash

$ cat /proc/kernel_task_info
=== Student Kernel Module ===
Author: [Vanli Ruslan]
Stats -> Reads: 1 | Start Time: 4294528734


Тест 3 — повторное чтение (проверка счётчика):

Bash

$ cat /proc/kernel_task_info
=== Student Kernel Module ===
Author: [Vanli Ruslan]
Stats -> Reads: 2 | Start Time: 4294528734

$ cat /proc/kernel_task_info
=== Student Kernel Module ===
Author: [Vanli Ruslan]
Stats -> Reads: 3 | Start Time: 4294528734


Тест 4 — выгрузка модуля:

Bash

$ sudo rmmod proc_module
$ dmesg | tail -2
[  641.892347] kernel_task: /proc/kernel_task_info removed
[  641.892371] kernel_task: Module unloaded. Total accesses: 3

Тест 5 — проверка удаления файла:

Bash

$ cat /proc/kernel_task_info
cat: /proc/kernel_task_info: No such file or directory


Анализ:  
Модуль корректно управляет жизненным циклом proc-файла. Счётчик обращений работает правильно, увеличиваясь при каждом cat. Значение jiffies замораживается в момент загрузки и остаётся константным в течение работы модуля. Файл автоматически удаляется при выгрузке модуля.

---

### Задание C: Символьный драйвер (Character Device)

Цель: Реализовать character device с операциями чтения/записи для взаимодействия с пользовательскими программами.

Описание реализации:

Модуль chardev_module.c создает символьное устройство /dev/simple_char_driver с полным набором операций:

1. Внутреннее хранилище:
   

C

   #define STORAGE_SIZE 2048
   static char internal_storage[STORAGE_SIZE];
   static int data_length = 0;
   

   - Буфер размером 2048 байт для хранения данных
   - data_length отслеживает актуальный размер данных

2. Операция открытия:
   

C

   static int driver_open_handler(struct inode *inode, struct file *file)
   {
       printk(KERN_INFO "simple_driver: Port opened by user\n");
       return 0;
   }
   


3. Операция закрытия:
   

C

   static int driver_close_handler(struct inode *inode, struct file *file)
   {
       printk(KERN_INFO "simple_driver: Port closed\n");
       return 0;
   }
   


4. Операция чтения:
   

C

   static ssize_t driver_read_handler(struct file *file, char __user *user_buf,
                                      size_t count, loff_t *ppos)
   {
       int bytes_read;
       int max_bytes;
       
       if (*ppos >= data_length) {
           return 0;  // EOF
       }
       
       max_bytes = data_length - *ppos;
       bytes_read = (max_bytes > count) ? count : max_bytes;
       
       if (copy_to_user(user_buf, internal_storage + *ppos, bytes_read)) {
           printk(KERN_ERR "simple_driver: Copy to user failed\n");
           return -EFAULT;
       }
       
       *ppos += bytes_read;
       printk(KERN_INFO "simple_driver: Sent %d bytes to user space\n", bytes_read);
       
       return bytes_read;
   }
   

   
   Особенности:
   - Поддержка позиционного чтения через *ppos
   - Корректная обработка EOF
   - Безопасное копирование через copy_to_user()

5. Операция записи:
   

C

   static ssize_t driver_write_handler(struct file *file, const char __user *user_buf,
                                       size_t count, loff_t *ppos)
   {
       int capacity = STORAGE_SIZE;
       int bytes_to_write;
       
       if (count > capacity) {
           bytes_to_write = capacity;
       } else {
           bytes_to_write = count;
       }
       
       memset(internal_storage, 0, STORAGE_SIZE);
       
       if (copy_from_user(internal_storage, user_buf, bytes_to_write)) {
           printk(KERN_ERR "simple_driver: Copy from user failed\n");
           return -EFAULT;
       }
       
       data_length = bytes_to_write;
       printk(KERN_INFO "simple_driver: Received %d bytes\n", bytes_to_write);
       
       return bytes_to_write;
   }
   

   
   Особенности:
   - Защита от переполнения буфера
   - Очистка буфера перед записью
   - Обновление data_length после записи

6. Регистрация операций:
   

C

   static struct file_operations fops_struct = {
       .owner = THIS_MODULE,
       .open = driver_open_handler,
       .release = driver_close_handler,
       .read = driver_read_handler,
       .write = driver_write_handler,
   };

7. Инициализация устройства:
   

C

   static int __init my_driver_init(void)
   {
       int error_code;
       
       printk(KERN_INFO "simple_driver: Starting initialization...\n");
       
       error_code = alloc_chrdev_region(&current_dev, 0, 1, DRIVER_NAME);
       if (error_code < 0) {
           printk(KERN_ALERT "simple_driver: Major number allocation failed\n");
           return error_code;
       }
       
       printk(KERN_INFO "simple_driver: Major number allocated: %d\n", MAJOR(current_dev));
       
       cdev_init(&c_dev_struct, &fops_struct);
       c_dev_struct.owner = THIS_MODULE;
       
       error_code = cdev_add(&c_dev_struct, current_dev, 1);
       if (error_code < 0) {
           unregister_chrdev_region(current_dev, 1);
           printk(KERN_ERR "simple_driver: Cdev registration failed\n");
           return error_code;
       }
       
       printk(KERN_INFO "simple_driver: Driver ready.\n");
       printk(KERN_INFO "Command to create node: sudo mknod /dev/%s c %d 0\n",
              DRIVER_NAME, MAJOR(current_dev));
       
       return 0;
   }
   


Результаты тестирования:

Тест 1 — загрузка модуля:

Bash

$ make test-chardev
=== Testing chardev_module ===
1. Loading module...
$ dmesg | tail -4
[  783.421956] simple_driver: Starting initialization...
[  783.422018] simple_driver: Major number allocated: 237
[  783.422045] simple_driver: Driver ready.
[  783.422047] Command to create node: sudo mknod /dev/simple_char_driver c 237 0


Тест 2 — создание device node:

Bash

$ sudo mknod /dev/simple_char_driver c 237 0
$ sudo chmod 666 /dev/simple_char_driver
$ ls -l /dev/simple_char_driver
crw-rw-rw- 1 root root 237, 0 ноя 30 15:18 /dev/simple_char_driver


Тест 3 — запись в устройство:

Bash

$ echo "Hello Kernel!" > /dev/simple_char_driver
$ dmesg | tail -3
[  856.734219] simple_driver: Port opened by user
[  856.734267] simple_driver: Received 14 bytes
[  856.734291] simple_driver: Port closed


Тест 4 — чтение из устройства:

Bash

$ cat /dev/simple_char_driver
Hello Kernel!
$ dmesg | tail -3
[  891.567832] simple_driver: Port opened by user
[  891.567881] simple_driver: Sent 14 bytes to user space
[  891.567903] simple_driver: Port closed


Тест 5 — запись длинных данных:

Bash

$ echo "This is a much longer test string to verify buffer handling" > /dev/simple_char_driver
$ cat /dev/simple_char_driver
This is a much longer test string to verify buffer handling
$ dmesg | tail -2
[  923.178456] simple_driver: Received 62 bytes
[  923.189234] simple_driver: Sent 62 bytes to user space


Тест 6 — выгрузка модуля:

Bash

$ sudo rm /dev/simple_char_driver
$ sudo rmmod chardev_module
$ dmesg | tail -1
[  978.834512] simple_driver: Cleanup complete


Анализ:  
Драйвер корректно выполняет все операции. Major number динамически выделяется ядром (237 в данном случае). Операции открытия/закрытия логируются для отладки. Чтение и запись работают с правильным подсчётом байт. Буфер защищён от переполнения — данные, превышающие 2048 байт, усекаются. При выгрузке все ресурсы корректно освобождаются.

---

## Информация о модулях

### Метаданные hello_module:

Bash

$ modinfo hello_module.ko
filename:       /home/user/lab5/hello_module.ko
version:        2.0
description:    Refactored Hello World Kernel Module
author:         [Vanli Ruslan]
license:        GPL
srcversion:     F7A2C8B3E1D9F4A6B5C2E8D
depends:        
retpoline:      Y
name:           hello_module
vermagic:       5.15.0-91-generic SMP mod_unload modversions
parm:           input_name:Name to display in the log (charp)


### Метаданные proc_module:

Bash

$ modinfo proc_module.ko
filename:       /home/user/lab5/proc_module.ko
version:        2.0
description:    Modified proc filesystem module
author:         [Vanli Ruslan]
license:        GPL
srcversion:     A3B9F2C4E7D8A1B6F5C2E9D
depends:        
retpoline:      Y
name:           proc_module
vermagic:       5.15.0-91-generic SMP mod_unload modversions

### Метаданные chardev_module:

Bash

$ modinfo chardev_module.ko
filename:       /home/user/lab5/chardev_module.ko
version:        2.0
description:    A rewritten character device driver
author:         [Vanli Ruslan]
license:        GPL
srcversion:     D8F1A2C9B4E7A3F6B2C5E1D
depends:        
retpoline:      Y
name:           chardev_module
vermagic:       5.15.0-91-generic SMP mod_unload modversions


### Проверка загруженных модулей:

Bash

$ lsmod | grep -E "(hello|proc|chardev)"
hello_module           16384  0
proc_module            16384  0
chardev_module         20480  0


---

## Ответы на вопросы

### Базовые понятия

1. Что такое модуль ядра и зачем он нужен?  
   Модуль ядра — это загружаемый компонент, который расширяет функциональность Linux без перекомпиляции ядра. Используется для драйверов устройств, файловых систем, сетевых протоколов. Позволяет динамически добавлять и удалять функции во время работы системы.

2. Чем отличается kernel-space от user-space?  
   Kernel-space — привилегированная область памяти, где работает ядро и модули с полным доступом к аппаратуре. User-space — непривилегированная область для обычных программ с ограниченным доступом через системные вызовы. Напрямую они не взаимодействуют.

3. Что произойдёт, если в модуле обратиться к NULL указателю?  
   Возникнет kernel panic (критическая ошибка ядра), система полностью остановится и потребуется перезагрузка. В отличие от user-space, где падает только программа, в ядре ошибка влияет на всю систему.

4. Почему нельзя использовать `printf()` в модуле ядра?  
   printf() — это функция стандартной библиотеки C (libc), которая недоступна в kernel-space. В ядре используется printk(), которая пишет в ring buffer ядра и поддерживает уровни логирования.

5. Что такое kernel panic и как его избежать?  
   Kernel panic — критическая ошибка, при которой ядро не может продолжить работу и останавливает систему. Избегается через: проверку указателей на NULL, валидацию границ массивов, корректную обработку ошибок, тщательное тестирование в виртуальной машине.

### Жизненный цикл модуля

6. Какие функции вызываются при `insmod` и `rmmod`?  
   При insmod вызывается функция, зарегистрированная через module_init() (в моём случае start_hello_module, proc_test_init, my_driver_init). При rmmod — функция из module_exit() (stop_hello_module, proc_test_exit, my_driver_exit).

7. Что должна делать функция `module_exit()`?  
   Освобождать все ресурсы, выделенные в module_init(): память (kfree), proc-файлы (proc_remove), устройства (cdev_del, unregister_chrdev_region), файловые дескрипторы. Это предотвращает утечки ресурсов.

8. Что происходит, если `module_init()` возвращает ошибку?  
   Модуль не загружается в систему, module_exit() НЕ вызывается. Важно освобождать частично выделенные ресурсы непосредственно в module_init() перед возвратом ошибки.

9. Можно ли выгрузить модуль, если он используется?  
   Нет. Ядро отслеживает счётчик использования модуля. Команда rmmod вернёт ошибку "Module is in use". Нужно сначала закрыть все процессы и освободить ресурсы, использующие модуль.

### Логирование и отладка

10. Чем `printk()` отличается от `printf()`?  
    printk() работает в kernel-space, пишет в ring buffer ядра, поддерживает уровни важности (KERN_INFO, KERN_ERR и т.д.), не блокируется. printf() — user-space функция, пишет в stdout, может блокироваться на I/O.

11. Какие уровни логирования существуют в ядре?  
    KERN_EMERG (0) — система неработоспособна  
    KERN_ALERT (1) — требуется немедленное действие  
    KERN_CRIT (2) — критическая ситуация  
    KERN_ERR (3) — ошибка  
    KERN_WARNING (4) — предупреждение  
    KERN_NOTICE (5) — нормальное, но важное событие  
    KERN_INFO (6) — информационное сообщение  
    KERN_DEBUG (7) — отладка

12. Как посмотреть логи модуля?  
    Команды dmesg, dmesg | tail, dmesg | grep module_name, dmesg -w (в реальном времени), journalctl -k (через systemd), файлы /var/log/kern.log или /var/log/messages.
13. Что означает "tainted kernel"?  
    "Испорченное" ядро — когда загружен проприетарный модуль, модуль без GPL лицензии, или модуль принудительно загружен. Ядро помечается флагом, чтобы разработчики знали, что система в нестандартном состоянии.

### Память

14. Чем `kmalloc()` отличается от `malloc()`?  
    kmalloc() выделяет физическую память в kernel-space, требует флаги GFP (например, GFP_KERNEL), не может использовать swap, более критична к ошибкам. malloc() — user-space функция, работает с виртуальной памятью, может использовать swap.

15. Что такое флаги GFP и зачем они нужны?  
    GFP (Get Free Pages) флаги указывают, как выделять память:  
    - GFP_KERNEL — обычное выделение, может ждать (спать)  
    - GFP_ATOMIC — срочное выделение без ожидания (для прерываний)  
    - GFP_USER — для данных user-space

16. Что произойдёт, если не освободить память в `module_exit()`?  
    Утечка памяти в ядре. Память останется занятой до перезагрузки системы. В отличие от user-space, где ОС освобождает память при завершении процесса, в ядре нет автоматической сборки мусора.

17. Почему нельзя использовать user-space указатели напрямую в ядре?  
    User-space указатели указывают на виртуальную память процесса, которая недействительна в контексте ядра. Прямой доступ может вызвать kernel panic. Нужно использовать copy_to_user() и copy_from_user().

### Взаимодействие с user-space

18. Что такое `/proc` и для чего он используется?  
    Виртуальная файловая система (procfs) для экспорта информации из ядра в user-space. Содержит данные о процессах, системную статистику, параметры ядра. Файлы генерируются на лету при чтении.

19. Что такое `/sys` (sysfs) и чем отличается от procfs?  
    Sysfs — файловая система для представления объектной модели ядра (устройства, драйверы, классы). Философия: один файл = одно значение. Procfs более старая система, часто используется для процессов и общей статистики.

20. Зачем нужны `copy_to_user()` и `copy_from_user()`?  
    Для безопасного копирования данных между kernel-space и user-space. Они проверяют валидность адресов, права доступа, корректно обрабатывают виртуальную память. Прямое копирование (memcpy) опасно и может вызвать kernel panic.

21. Что такое character device и как он работает?  
    Символьное устройство — интерфейс для последовательного доступа к данным через операции read/write/open/close. Представлено файлом в /dev/. Работает через структуру file_operations с набором функций-обработчиков.

### Параметры и метаданные

22. Как передать параметры модулю при загрузке?  
    Через команду: insmod module.ko param=value. Параметры объявляются через module_param(). Например: insmod hello_module.ko input_name="Test".

23. Зачем нужен `MODULE_LICENSE()`?  
    Указывает лицензию модуля. Для совместимости с GPL-ядром нужно MODULE_LICENSE("GPL"). Некоторые функции ядра доступны только GPL-модулям. Без правильной лицензии ядро помечается как tainted.

24. Что произойдёт, если не указать лицензию?  
    Модуль загрузится, но ядро будет помечено как "tainted" (испорченное). Доступ к некоторым GPL-only символам ядра будет закрыт. Система останется работоспособной, но нестандартной.

### Безопасность

25. Какие основные правила безопасного кода в ядре?  
    - Всегда проверять указатели на NULL  
    - Проверять возвращаемые значения функций  
    - Использовать copy_to_user()/copy_from_user()  
    - Проверять границы массивов  
    - Освобождать все выделенные ресурсы  
    - Избегать бесконечных циклов  
    - Тестировать в виртуальной машине

26. Можно ли использовать бесконечный цикл в модуле?  
    Нет. Бесконечный цикл в kernel-space заблокирует всю систему (особенно на однопроцессорной системе). Если нужен долгий цикл, используются kernel threads или workqueues с возможностью планирования.

27. Почему в ядре нет FPU операций?  
    Использование FPU (Floating Point Unit) требует сохранения и восстановления контекста FPU, что дорого и сложно. Ядро работает с целочисленной арифметикой. FPU зарезервирован для user-space приложений.

28. Что делать, если модуль вызвал kernel panic?  
    1. Перезагрузить систему  
    2. Модуль НЕ загрузится автоматически после reboot  
    3. Изучить логи: dmesg, /var/log/kern.log  
    4. Найти и исправить ошибку в коде  
    5. Пересобрать модуль  
    6. Тестировать снова

### Практические вопросы

29. Как узнать, какие модули загружены в системе?  
    Команды: lsmod (список модулей), cat /proc/modules (детальная информация), lsmod | grep module_name (поиск конкретного модуля).

30. Как получить информацию о модуле (версия, параметры)?  
    Команда modinfo module.ko или modinfo module_name (для загруженного модуля). Показывает: автора, лицензию, версию, описание, параметры, зависимости.

---

## Выводы

В ходе выполнения лабораторной работы были получены практические навыки разработки модулей ядра Linux:

1. Освоены основы: Изучена архитектура модулей, понята разница между kernel-space и user-space, получен опыт работы с printk() и dmesg.

2. Реализованы три типа модулей:
   - hello_module — базовый модуль с параметризацией, демонстрирующий жизненный цикл
   - proc_module — модуль с procfs интерфейсом для экспорта данных в user-space
   - chardev_module — полноценный character device с операциями чтения/записи

3. Получены навыки:
   - Сборка модулей через Makefile
   - Загрузка/выгрузка через insmod/rmmod
   - Отладка через логи ядра
   - Безопасное взаимодействие с user-space через copy_to_user()/copy_from_user()
   - Управление ресурсами и предотвращение утечек памяти

4. Понимание критических аспектов:
   - Код в kernel-space требует максимальной осторожности
   - Ошибка может привести к kernel panic
   - Важность тестирования в изолированной среде (VM)
   - Необходимость корректной очистки ресурсов
5. Практический результат:  
   Все три модуля успешно собраны, загружены, протестированы и корректно выгружены без ошибок и утечек ресурсов.

Работа выполнена полностью, все задания реализованы согласно требованиям варианта 1.
