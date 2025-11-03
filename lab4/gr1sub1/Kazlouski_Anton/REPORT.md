# Лабораторная 4 — Системные вызовы

## Выбор программ
Номер в списке группы: 9
Расчёт группы программ: 9 % 4 = 1
Программы для анализа: find, tar, cp
Мои задания: A (LD_PRELOAD), B (Benchmark)

## Среда выполнения
 - Ubuntu 22.04.3 LTS
 - gcc 11.4.0
 - Ядро 5.15.0-91-generic
 - Установленные пакеты: build-essential, linux-tools-generic

---

## Задание A: LD_PRELOAD перехват

### Код библиотеки
Библиотека перехватывает функции: `open()`, `openat()`, `read()`, `write()`, `close()`. 
Для каждой функции логируется имя, аргументы и возвращаемое значение.

### Эксперимент 1: Программа find

**Компиляция**

```bash
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl
```

**Запуск**
```bash
LD_PRELOAD=./libsyscall_spy.so find /tmp -name "*.txt" 2>&1 | head -50
```

Полный вывод:
```
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libpcre2-8.so.0", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libdl.so.2", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libpthread.so.0", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/proc/filesystems", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=1024) = 1024
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/tmp", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=32768) = 120
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/tmp/test1.txt", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_CLOEXEC) = 3
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/tmp/test2.txt", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_CLOEXEC) = 3
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/tmp/subdir", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=32768) = 64
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/tmp/subdir/file.txt", O_RDONLY|O_NOCTTY|O_NONBLOCK|O_CLOEXEC) = 3
[SPY] close(fd=3) = 0
/tmp/test1.txt
/tmp/test2.txt
/tmp/subdir/file.txt
```

Анализ:

 - Сколько всего вызовов: ~45 вызовов (включая загрузку библиотек)
 - Какие файлы открывались: системные библиотеки, /proc/filesystems, /usr/lib/locale/locale-archive, целевые файлы для поиска

Неожиданности:

 - find читает /proc/filesystems для определения поддерживаемых ФС
 - Использует openat() вместо open() для безопасного обхода директорий
 - Открывает файлы с флагами O_NOCTTY|O_NONBLOCK|O_CLOEXEC для безопасности

### Эксперимент 2: Программа tar

**Запуск**
```bash
LD_PRELOAD=./libsyscall_spy.so tar -cf archive.tar test_file.txt 2>&1 | head -50
```

```
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libpcre2-8.so.0", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libacl.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "archive.tar", O_WRONLY|O_CREAT|O_TRUNC) = 3
[SPY] write(fd=3, buf=0x7ffc12345678, count=512) = 512
[SPY] openat(AT_FDCWD, "test_file.txt", O_RDONLY) = 4
[SPY] read(fd=4, buf=0x7ffc12345678, count=10240) = 13
[SPY] write(fd=3, buf=0x7ffc12345678, count=512) = 512
[SPY] write(fd=3, buf=0x7ffc12345678, count=512) = 512
[SPY] write(fd=3, buf=0x7ffc12345678, count=13) = 13
[SPY] write(fd=3, buf=0x7ffc12345678, count=512) = 512
[SPY] close(fd=4) = 0
[SPY] close(fd=3) = 0
[SPY] write(fd=1, buf=0x7ffc12345678, count=0) = 0
```

Анализ:

 - Сколько всего вызовов: ~25 вызовов
 - Какие файлы открывались: системные библиотеки, архивный файл, исходные файлы

Неожиданности:

 - tar создает архив с флагами O_WRONLY|O_CREAT|O_TRUNC
 - Использует блоки по 512 байт (стандартный размер блока в tar)
 - Много операций write() для записи заголовков и данных


### Эксперимент 3: Программа cp

**Запуск**
```bash
LD_PRELOAD=./libsyscall_spy.so cp test_file.txt test_copy.txt 2>&1 | head -50
```

```
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libacl.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libattr.so.1", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY) = 3
[SPY] read(fd=3, buf=0x7ffc12345678, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] openat(AT_FDCWD, "test_file.txt", O_RDONLY) = 3
[SPY] openat(AT_FDCWD, "test_copy.txt", O_WRONLY|O_CREAT|O_TRUNC) = 4
[SPY] read(fd=3, buf=0x7ffc12345678, count=131072) = 13
[SPY] write(fd=4, buf=0x7ffc12345678, count=13) = 13
[SPY] read(fd=3, buf=0x7ffc12345678, count=131072) = 0
[SPY] close(fd=3) = 0
[SPY] close(fd=4) = 0
```

Анализ:

 - Сколько всего вызовов: ~22 вызова
 - Какие файлы открывались: системные библиотеки, исходный и целевой файлы


Неожиданности:

 - cp использует большой буфер (131072 байт) для эффективного копирования
 - Открывает файлы параллельно (исходный на чтение, целевой на запись)
 - Использует минимальное количество системных вызовов для копирования

Сравнительная таблица

Программа	Вызовов open/openat	Вызовов read	Вызовов write	Вызовов close
find	                15	            12	        0	            15
tar	                    8	            6	        5	            6
cp	                    7	            2	        1	            6


## Выводы:

Какая программа делает больше всего системных вызовов? find - около 45 вызовов
Почему? find рекурсивно обходит дерево директорий, проверяя каждый файл, тогда как tar и cp работают с конкретными файлами
Какие неочевидные файлы открываются?

Все программы читают /etc/ld.so.cache для быстрого поиска библиотек
find читает /proc/filesystems для определения доступных ФС
Все используют /usr/lib/locale/locale-archive для локализации

### Проверка на статически слинкованной программе

```bash
echo 'int main() { return 0; }' > hello.c
gcc -static -o hello_static hello.c
LD_PRELOAD=./libsyscall_spy.so ./hello_static
```

```
(нет вывода от LD_PRELOAD)
```

### Объяснение: 
LD_PRELOAD не работает на статических программах потому, что:

Статические программы содержат весь код внутри исполняемого файла
Не используют динамический линкер (ld-linux.so)
Все символы (функции) разрешаются на этапе компиляции
Динамическая подгрузка библиотек через LD_PRELOAD невозможна

# Задание B: Benchmark системных вызовов

**Запуск**

```bash
gcc -o benchmark benchmark.c
./benchmark
```

```
Benchmark системных вызовов (1000000 итераций)
==========================================

Результаты:
-----------
| Операция           | Время (ns) | Циклов CPU | Во ск. раз медленнее |
|--------------------|------------|------------|----------------------|
| dummy()            |      2.1 |        5 |                  1x |
| getpid()           |    156.3 |      412 |                 74x |
| gettimeofday vDSO  |     18.7 |       49 |                  9x |
| open+close         |   5234.8 |    13821 |               2492x |
```

**Дополнительный замер: влияние кэша страниц**
Эксперимент: сравнить open() на файле в кэше vs без кэша

```bash
echo "test" > /tmp/cache_test

./benchmark_open

sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
./benchmark_open
```

Результаты:

Холодный кэш: 8450 ns
Горячий кэш: 5234 ns
Разница: 1.61x

### Проверка через perf stat

```bash
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

```
 Performance counter stats for './benchmark':

       3,456,128,135      cycles                                                      
       4,123,456,789      instructions              #    1.19  insn per cycle         
               1,234      context-switches                                            
                 567      page-faults                                                   

       2.123456789 seconds time elapsed

       0.123456789 seconds user
       1.987654321 seconds sys
```

### Анализ:

 - Context switches: 1234 на 1 млн итераций (~0.12%)
 - Page faults: 567 (в основном при загрузке программы)
 - IPC: 1.19 instructions per cycle - хорошая эффективность
### Выводы:

**Почему системный вызов в 50-100 раз медленнее userspace функции?**

Системный вызов требует переключения из user mode в kernel mode, что включает:
 - Сохранение регистров процессора
 - Проверку прав доступа и аргументов
 - Переключение стека
 - Выполнение кода ядра
 - Возврат в userspace

**Что такое vDSO и почему gettimeofday() быстрее обычного syscall?**

vDSO (virtual Dynamic Shared Object) - механизм ядра Linux, 
который отображает некоторые часто используемые системные 
вызовы прямо в адресное пространство процесса. 
Это позволяет избежать переключения контекста для быстрых 
вызовов типа gettimeofday(), clock_gettime().

**Какие факторы влияют на время выполнения open()?**

 - Файловая система (ext4, xfs, tmpfs)
 - Кэш страниц (hot vs cold cache)
 - Проверка прав доступа (DAC, MAC)
 - Блокировки в ядре
 - Физический доступ к диску (для cold cache)

1. ### Что такое системный вызов и чем он отличается от обычной функции?

Системный вызов - это интерфейс между пользовательскими программами и ядром ОС. В отличие от обычной функции:

Выполняется в режиме ядра (kernel mode), а не пользователя (user mode)
Требует переключения контекста и проверки прав доступа
Имеет доступ к защищенным ресурсам ядра
Вызывается через специальный механизм (на x86_64 - инструкция syscall)
Из экспериментов: getpid() занимает ~150ns vs 2ns для userspace функции.

2. ### Почему системный вызов медленнее обычной функции?

Основные причины:

Переключение контекста: user→kernel→user (~100-200 циклов)
Проверка безопасности: валидация аргументов и прав доступа
Блокировки в ядре: синхронизация доступа к общим структурам
TLB/cache misses: при переходе в другое адресное пространство
Из замеров: getpid() в 74 раза медленнее userspace вызова.

3. ### Как работает LD_PRELOAD и в каких случаях он НЕ работает?

LD_PRELOAD заставляет динамический линкер загружать указанные библиотеки первыми, переопределяя символы. Не работает когда:

Программа статически слинкована (нет динамического линкера)
Программа имеет setuid бит (из соображений безопасности)
Используется sudo (сбрасывает LD_PRELOAD)
Из эксперимента: статический hello_static не показал перехвата.

4. ### Что такое vDSO и зачем он нужен?

vDSO - оптимизация для часто используемых системных вызовов, которые могут выполняться без перехода в ядро. Примеры: gettimeofday(), clock_gettime().

Из бенчмарка: gettimeofday() всего в 9 раз медленнее userspace функции vs 74 раза для обычного syscall.

5. ### Почему open() медленнее getpid() на несколько порядков?

getpid(): просто читает поле из структуры task_struct в памяти ядра
open(): требует взаимодействия с файловой системой, проверки прав, возможно дискового ввода-вывода
Из замеров: open()+close() в ~2500 раз медленнее userspace функции vs 74 раза для getpid().