# Лабораторная 4 — Системные вызовы

## Выбор программ
Номер в списке группы: 9.  
Расчёт группы программ: 9 % 4 = 1.  
Программы для анализа: `find`, `tar`, `cp`.  
Мои задания: A (LD_PRELOAD), B (Benchmark).  

## Среда выполнения
- ОС: Ubuntu 24.04.3 LTS
- Ядро: 6.14.0-33-generic
- Процессор: AMD Ryzen 5 5600H with Radeon Graphics
- Компилятор: gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.  
This is free software; see the source for copying conditions.  There is NO  
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

---

## Задание A: LD_PRELOAD перехват

### Код библиотеки
Библиотека `libsyscall_spy.so` перехватывает системные вызовы и библиотечные функции:
- **Низкоуровневые syscalls:** `open`, `openat`, `read`, `write`, `close`,
- **Улучшения:** декодирование флагов `open()`, вывод содержимого `write()`, обработка ошибок
dlsym(RTLD_NEXT, "open") - находит указатель на оригинальную функцию open из системной библиотеки  
RTLD_NEXT - специальный флаг, который означает "найди следующее определение этой функции в цепочке библиотек"  
static указатель - сохраняется между вызовами, чтобы не искать функцию каждый раз  
Библиотека загружается первой благодаря LD_PRELOAD, поэтому все вызовы идут сначала в наши функции.  

```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// ==================== ДЕКОДИРОВАНИЕ ФЛАГОВ OPEN() ====================

const char* decode_open_flags(int flags) {
    static char buffer[256];
    buffer[0] = '\0';
    
    if (flags & O_RDONLY) strcat(buffer, "O_RDONLY|");
    if (flags & O_WRONLY) strcat(buffer, "O_WRONLY|");
    if (flags & O_RDWR)   strcat(buffer, "O_RDWR|");
    if (flags & O_CREAT)  strcat(buffer, "O_CREAT|");
    if (flags & O_EXCL)   strcat(buffer, "O_EXCL|");
    if (flags & O_TRUNC)  strcat(buffer, "O_TRUNC|");
    if (flags & O_APPEND) strcat(buffer, "O_APPEND|");
    if (flags & O_DIRECTORY) strcat(buffer, "O_DIRECTORY|");
    
    // Убираем последний символ '|'
    size_t len = strlen(buffer);
    if (len > 0) buffer[len-1] = '\0';
    
    return buffer;
}

const char* decode_dirfd(int dirfd) {
    if (dirfd == AT_FDCWD) return "AT_FDCWD";
    
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", dirfd);
    return buffer;
}

// ==================== ПЕРЕХВАТ OPEN() ====================

int open(const char *pathname, int flags, ...) {
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_open(pathname, flags, mode);

    const char* flags_str = decode_open_flags(flags);
    fprintf(stderr, "[SPY] open(\"%s\", %s, 0%o) = %d", 
            pathname, flags_str, mode, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ OPENAT() ====================

int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_openat(dirfd, pathname, flags, mode);

    const char* dirfd_str = decode_dirfd(dirfd);
    const char* flags_str = decode_open_flags(flags);
    
    fprintf(stderr, "[SPY] openat(%s, \"%s\", %s, 0%o) = %d", 
            dirfd_str, pathname, flags_str, mode, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ READ() ====================

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd", 
            fd, buf, count, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ WRITE() С ВЫВОДОМ СОДЕРЖИМОГО ====================

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);

    // Избегаем рекурсии - не логируем запись в stderr (fd=2)
    if (fd != 2) {
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd", 
                fd, buf, count, result);
        
        // Выводим содержимое для небольших записей в stdout (fd=1)
        if (fd == 1 && count > 0 && count <= 100) {
            fprintf(stderr, " DATA: \"");
            // Безопасный вывод - заменяем непечатаемые символы
            for (size_t i = 0; i < count && i < 50; i++) {
                char c = ((char*)buf)[i];
                if (c >= 32 && c <= 126) {
                    fputc(c, stderr);
                } else {
                    fprintf(stderr, "\\x%02x", (unsigned char)c);
                }
            }
            if (count > 50) fprintf(stderr, "...");
            fprintf(stderr, "\"");
        }
        
        if (result == -1) {
            fprintf(stderr, " [ERROR: %s]", strerror(errno));
        }
        fprintf(stderr, "\n");
    }

    return result;
}

// ==================== ПЕРЕХВАТ CLOSE() ====================

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d", fd, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}
```

### Эксперимент 1: find /tmp -name "*.txt"
**Команда:**
```bash
LD_PRELOAD=./libsyscall_spy.so find /tmp -name "*.txt" 2> logs/find_log.txt
```

**Полный вывод (первые 30-50 строк):**
```
[SPY] open(".", , 00) = 3
[SPY] openat(AT_FDCWD, "/tmp", O_DIRECTORY, 00) = 4
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-apache2.service-aTBfGJ", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-apache2.service-aTBfGJ’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-timesyncd.service-PKjdRm", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-timesyncd.service-PKjdRm’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-bluetooth.service-m0Im6I", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-bluetooth.service-m0Im6I’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ubuntu-advantage-desktop-daemon.service-Hxno63", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ubuntu-advantage-desktop-daemon.service-Hxno63’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-power-profiles-daemon.service-aJRBNN", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-power-profiles-daemon.service-aJRBNN’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-upower.service-J03DL9", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-upower.service-J03DL9’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ModemManager.service-UHGP9J", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ModemManager.service-UHGP9J’: Permission denied
[SPY] openat(5, ".ICE-unix", O_DIRECTORY, 00) = 6
[SPY] close(fd=4) = 0
[SPY] close(fd=7) = 0
[SPY] close(fd=4) = 0
[SPY] openat(5, ".X11-unix", O_DIRECTORY, 00) = 6
[SPY] close(fd=4) = 0
[SPY] close(fd=7) = 0
[SPY] close(fd=4) = 0
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-logind.service-pXfTzQ", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-logind.service-pXfTzQ’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-switcheroo-control.service-ZolWjO", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-switcheroo-control.service-ZolWjO’: Permission denied
[SPY] openat(5, ".XIM-unix", O_DIRECTORY, 00) = 6
[SPY] close(fd=7) = 0
[SPY] close(fd=4) = 0
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-resolved.service-5PjFVp", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-resolved.service-5PjFVp’: Permission denied
[SPY] openat(5, ".font-unix", O_DIRECTORY, 00) = 6
[SPY] close(fd=7) = 0
[SPY] close(fd=4) = 0
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-polkit.service-CwquFZ", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-polkit.service-CwquFZ’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-colord.service-XrbziM", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-colord.service-XrbziM’: Permission denied
[SPY] openat(5, "snap-private-tmp", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/snap-private-tmp’: Permission denied
[SPY] openat(5, "tmpzwft9jp6", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/tmpzwft9jp6’: Permission denied
[SPY] openat(5, "{9909ABC7-C9CC-4E00-ABBC-FDFA6DB67A38}", O_DIRECTORY, 00) = 6
[SPY] close(fd=4) = 0
[SPY] close(fd=7) = 0
[SPY] close(fd=4) = 0
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-fwupd.service-onhWdg", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-fwupd.service-onhWdg’: Permission denied
[SPY] openat(5, "systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-oomd.service-8zFsRd", O_DIRECTORY, 00) = -1 [ERROR: Permission denied]
find: ‘/tmp/systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-oomd.service-8zFsRd’: Permission denied
[SPY] close(fd=5) = 0
[SPY] close(fd=4) = 0
[SPY] close(fd=3) = 0
```

**Анализ:**
- Сколько всего вызовов: 39
- Какие файлы открывались:  
Системные директории: .ICE-unix, .X11-unix, .XIM-unix, .font-unix  
Системные service-директории: systemd-private-* (много ошибок доступа)  
Целевая директория: {9909ABC7-C9CC-4E00-ABBC-FDFA6DB67A38}  
Найденные файлы: cpptools.txt, cpptools-srv8538D0508E9348CDA171CD7505B7F77D.txt  
- Неожиданности:  
Много ошибок доступа - find пытается читать системные директории, хотя знает, что нет прав
Использование O_DIRECTORY - безопасное открытие только директорий
Быстрое закрытие дескрипторов - эффективное управление ресурсами

### Эксперимент 2: tar -cf archive.tar file.txt
**Команда:**
```bash
LLD_PRELOAD=./libsyscall_spy.so tar -cf testart.tar test_file.txt 2> logs/tar_log.txt
```

**Полный вывод (первые 30-50 строк):**
```
[SPY] read(fd=4, buf=0x55d0ca562200, count=5) = 5
[SPY] close(fd=4) = 0
[SPY] write(fd=3, buf=0x55d0ca562000, count=10240) = 10240
[SPY] close(fd=3) = 0
```

**Анализ:**
- Сколько всего вызовов: 4
- Какие файлы открывались:
В логах нет информации об открытых файлах.
- Неожиданности:
Отсутствие открытия файлов.  
Разница размеров - читает 5 байт, пишет 10240 байт.  
Минимальное количество вызовов для создания архива.  
Файлы открываются каким-то другим способом - не через перехваченные функции.  

### Эксперимент 3: cp source.txt dest.txt
**Команда:**
```bash
LD_PRELOAD=./libsyscall_spy.so cp test_file.txt copy_test.txt 2> logs/cp_log.txt
```

**Полный вывод (первые 30-50 строк):**
```
[SPY] open("copy_test.txt", O_DIRECTORY, 00) = -1 [ERROR: No such file or directory]
[SPY] open("test_file.txt", , 00) = 3
[SPY] openat(AT_FDCWD, "copy_test.txt", O_WRONLY|O_CREAT|O_EXCL, 0664) = 4
[SPY] close(fd=4) = 0
[SPY] close(fd=3) = 0
```

**Анализ:**
- Сколько всего вызовов: 5
- Какие файлы открывались:
copy_test.txt - проверка, не директория ли (с O_DIRECTORY).  
test_file.txt - исходный файл (чтение).  
copy_test.txt - целевой файл (создание с O_CREAT|O_EXCL).  
- Неожиданности:
Отсутствие read/write при успешном копировании файла.  
Проверка цели на директорию перед копированием.  
Файл копируется без видимых операций чтения/записи - используется другой механизм.  

### Сравнительная таблица
| Программа | Вызовов open/openat | Вызовов read | Вызовов write | Вызовов close |
|-----------|---------------------|--------------|---------------|---------------|
| find      | 23                  | 0            | 0             | 16            |
| tar       | 0                   | 1            | 1             | 2             |
| cp        | 3                   | 0            | 0             | 2             |

**Выводы:**
- Какая программа делает больше всего системных вызовов?
find
- Почему? (природа работы: файловая vs сетевая vs компиляция)
Много операций с файловой системой для рекурсивного поиска.
- Какие неочевидные файлы открываются? (библиотеки, конфиги, /proc, /sys)
1. Системные service-директории (самые неочевидные)  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-apache2.service-aTBfGJ  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-timesyncd.service-PKjdRm  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-bluetooth.service-m0Im6I  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ubuntu-advantage-desktop-daemon.service-Hxno63  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-power-profiles-daemon.service-aJRBNN  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-upower.service-J03DL9  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-ModemManager.service-UHGP9J  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-logind.service-pXfTzQ  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-switcheroo-control.service-ZolWjO  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-resolved.service-5PjFVp  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-polkit.service-CwquFZ  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-colord.service-XrbziM  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-fwupd.service-onhWdg  
systemd-private-c367664c13e7416c8f45c54b30e7c1d0-systemd-oomd.service-8zFsRd  
Временные директории systemd для изоляции временных файлов разных сервисов. Каждый сервис получает свою приватную директорию в /tmp. Обычный пользователь не знает о существовании этих директорий, они создаются автоматически systemd и обычно скрыты от пользователя.
2. X11 и графические системные файлы
.ICE-unix  
.X11-unix  
.XIM-unix  
.font-unix  
Сокеты для межпроцессного взаимодействия X Window System:  
.ICE-unix - ICE (Inter-Client Exchange) protocol sockets  
.X11-unix - X11 server sockets  
.XIM-unix - X Input Method sockets  
.font-unix - Font server sockets  
Это низкоуровневые системные сокеты, которые обычно не видны пользователям и используются графической подсистемой.  
3. Snap-изоляция  
snap-private-tmp  
Временные директории для изоляции snap-пакетов. Snap-пакеты используют собственные изолированные временные директории, о которых обычный пользователь не знает.  
4. Временные системные файлы
tmpzwft9jp6  
Временная директория, созданная какой-то системной утилитой или приложением. Имя выглядит как случайно сгенерированное, пользователь не знает о его назначении.  

### Проверка на статически слинкованной программе
**Команда:**
```bash
# Создаём статическую программу
gcc -static -o hello_static hello.c
LD_PRELOAD=./libsyscall_spy.so ./hello_static
```

**Результат:**
```
Hello, static world!
```

Анализ:
Программа выполнилась успешно и вывела сообщение.  
Логи перехвата отсутствуют - это ожидаемое поведение.  
Перехват системных вызовов через LD_PRELOAD не работает для статических программ.  

**Объяснение:** почему LD_PRELOAD не работает на статических программах?  
Статически слинкованные программы включают весь необходимый код (включая реализацию системных вызовов) непосредственно в исполняемый файл. При компиляции:  
Вызовы функций разрешаются на этапе линковки  
Нет динамической загрузки библиотек через ld-linux  
LD_PRELOAD не применяется, так как нет динамического линкера, который мог бы загрузить нашу библиотеку  

---

## Задание B: Benchmark системных вызовов

### Исходный код
```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <x86intrin.h>

// Userspace функция
int dummy_function() {
    return 42;
}

// Измерение времени в наносекундах
uint64_t get_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main() {
    const int iterations = 1000000;
    uint64_t total_time, total_cycles;
    
    printf("=== System Call Benchmark ===\n");
    printf("Iterations: %d\n\n", iterations);
    
    // 1. Userspace функция
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        dummy_function();
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("dummy() userspace: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 2. getpid()
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        getpid();
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("getpid() syscall: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 3. open()+close()
    system("echo 'test' > /tmp/benchmark_file");
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        int fd = open("/tmp/benchmark_file", O_RDONLY);
        if (fd != -1) close(fd);
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    system("rm -f /tmp/benchmark_file");
    printf("open()+close(): %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 4. gettimeofday() vDSO
    struct timeval tv;
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        gettimeofday(&tv, NULL);
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("gettimeofday() vDSO: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    return 0;
}
```

### Таблица результатов (1 миллион итераций)

| Операция                  | Среднее время (ns) | Циклов CPU   | Во ск. раз медленнее userspace |
|---------------------------|--------------------|--------------|--------------------------------|
| dummy() userspace         | 1462               | 58           | 1x (baseline)                  |
| getpid()                  | 1808               |18446744068860| 1.23666x                       |
| open("/tmp/test")+close() | 5124               | 11991        | 3.5x                           |
| gettimeofday() vDSO       | 2428               | 3948         | 1.66x                          |

**Команды для воспроизведения:**
```bash
./benchmark
```

**Вывод программы (полностью):**
```
=== System Call Benchmark ===
Iterations: 1000000

dummy() userspace: 1462 ns, 58 cycles
getpid() syscall: 1808 ns, 18446744068860 cycles
open()+close(): 5124 ns, 11991 cycles
gettimeofday() vDSO: 2428 ns, 3948 cycles
```

### Дополнительный замер: влияние кэша страниц
**Эксперимент:** сравнить `open()` на файле в кэше vs без кэша

```bash
# Сброс кэша (требует root)
sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches

# Первый запуск (холодный кэш)
./benchmark_open

# Второй запуск (горячий кэш)
./benchmark_open
```

**Результаты:**
- Холодный кэш: 4945 ns
- Горячий кэш: 5071 ns
- Разница:  0,975x

### Проверка через perf stat
```bash
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

**Вывод:**
```bash
=== System Call Benchmark ===
Iterations: 1000000

dummy() userspace: 1222 ns, 28 cycles
getpid() syscall: 1327 ns, 503 cycles
open()+close(): 3648 ns, 7687 cycles
gettimeofday() vDSO: 2432 ns, 3964 cycles

 Performance counter stats for './benchmark':

    56 576 071 971      cycles                                                                
    14 282 839 997      instructions                     #    0,25  insn per cycle            
               171      context-switches                                                      
               265      page-faults                                                           

      13,466910375 seconds time elapsed

       0,915167000 seconds user
      12,541769000 seconds sys
```

**Анализ:**
- Сколько context switches на миллион итераций?
171 на 4 миллиона => 42 на миллион  
Очень низкий показатель - всего 0.004% операций вызвали переключение контекста  
Linux использует оптимизации - системные вызовы выполняются пакетами без постоянных переключений  
На одну операцию приходится ~0.00004 переключений контекста  
- Сколько page faults?
265 страничных нарушений  
Умеренное количество - в основном загрузка библиотек и бинарника  
Программа линейная, данные уже в памяти после загрузки  
Всего 0.007% операций вызвали page faults  
- IPC (instructions per cycle) — высокий или низкий?  
0.25 инструкций за цикл  
Очень низкий показатель  
Программа проводит много времени в ожидании системных вызовов  
Процессор простаивает в ожидании завершения операций ввода-вывода  

**Выводы:**
- Почему системный вызов в 50-100 раз медленнее userspace функции?  
Context switch overhead: Переключение user→kernel→user занимает ~100-200ns  
Проверки безопасности: Ядро проверяет права доступа, валидность аргументов  
Сохранение состояния: Сохранение/восстановление регистров CPU  
Конкретные цифры: getpid() в 18 раз медленнее по циклам (503 vs 28), но всего на 8% по времени (1327ns vs 1222ns) из-за оптимизаций  
- Что такое vDSO и почему gettimeofday() быстрее обычного syscall?  
vDSO (Virtual Dynamic Shared Object) - механизм выполнения часто используемых syscalls в userspace  
Избегает context switch: gettimeofday() выполняется без перехода в ядро  
Но в моём случае gettimeofday() медленнее getpid()  
- Какие факторы влияют на время выполнения open()? (файловая система, кэш, права)
Файловая система: Поиск inode, чтение метаданных  
Кэш: Состояние dentry cache и page cache  
Права доступа: Проверка разрешений для каждого файла  
Блокировки: Синхронизация в ядре при доступе к файловой системе  

---

## Ответы на обязательные вопросы

### 1. Что такое системный вызов и чем он отличается от обычной функции?
Системный вызов - это интерфейс между userspace и kernel space. В отличие от обычной функции:  
Требует переключения контекста (user→kernel→user)  
Выполняется с привилегиями ядра  
Подвергается проверкам безопасности  

### 2. Почему системный вызов медленнее обычной функции?
Проверки прав доступа и безопасности  
Сохранение/восстановление состояния  
**Конкретные цифры:** getpid() не сильно медленнее dummy()-функции

### 3. Как работает LD_PRELOAD и в каких случаях он НЕ работает?
**Работает:** Подмена функций в динамически линкованных программах  
**Не работает:** 
Статические программы (проверено в эксперименте)  
SUID-программы (из соображений безопасности)  
Программы, использующие прямые syscalls через `syscall()`

### 4. Что такое vDSO и зачем он нужен?
**Ответ:** Virtual Dynamic Shared Object - механизм для ускорения частых syscalls:
Избегает context switch для вызовов типа `gettimeofday()`
**Результаты:** gettimeofday() всего в 2 раза медленнее userspace-функции  

### 5. Почему open() медленнее getpid() на несколько порядков?
`getpid()`: чтение одного поля из структуры процесса в памяти  
`open()`: парсинг пути, проверка прав, работа с файловой системой, возможные операции с диском  
**Экспериментальные данные:** open()+close() в 3 раза медленнее userspace-функции  

---

## Общие выводы

- Что нового узнали о работе syscalls?
Обнаружили, что современные программы используют специализированные системные вызовы (copy_file_range), а не только классические read/write  
Некоторые "системные вызовы" вообще не требуют перехода в ядро, выполняются в userspace  
Системный вызов не просто функция, это полноценное переключение между user mode и kernel mode с сохранением состояния  
- Какие инструменты показались наиболее полезными?  
LD_PRELOAD - инструмент для перехвата и отладки, позволяет заглянуть внутрь работающих программ без изменения их кода  
strace - показывает какие syscalls на самом деле вызываются  
perf stat - даёт глубокое понимание производительности на уровне процессорных метрик (хотя в моём случае были технические сложности)  
- Какие трудности возникли?
Несовместимость perf - современные ядра Linux обновляются быстрее, чем пакеты в репозиториях  
- Как понимание границы userspace↔kernel поможет в дальнейшем изучении Linux?
Понимание что программы не работают в вакууме - они взаимодействуют с сложной экосистемой ядра, и это взаимодействие часто определяет их реальную производительность больше, чем качество самого кода

---

## Воспроизводимость

### Команды для полного воспроизведения

```bash
# Сборка В)
gcc -O2 -o benchmark src/benchmark.c

# Задание A
LD_PRELOAD=./libsyscall_spy.so find /tmp -name "*.txt"

# Задание B
./benchmark
```

## Использование ИИ
Искусственный интеллект использовался для объяснения логики перехвата системных вызовов, написания benchmark и комманд для выполнения в терминале.
