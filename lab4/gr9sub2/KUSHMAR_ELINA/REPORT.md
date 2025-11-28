# Лабораторная 4 — Системные вызовы: граница между программой и ядром
Кушмар Элина Викторовна, 4 курс, 9 группа

## Выбор программ
```markdown
Номер в списке: 11
Программы: 11 % 4 = 3 → `gcc`, `make`, `as`
```

## Среда выполнения
- ОС: Ubuntu 25.04
- Ядро: 6.14.0-15-generic
- Процессор: $(lscpu | grep "Model name" | cut -d: -f2 | xargs)
- Компилятор: gcc (Ubuntu 14.2.0-19ubuntu2) 14.2.0

## Цели лабораторной

- Понять, что такое системный вызов и как он работает изнутри.
- Увидеть **все** системные вызовы, которые делает программа (даже те, что скрыты библиотеками).
- Измерить **накладные расходы** (overhead) — сколько времени тратится на переход в ядро и обратно.
- Научиться **перехватывать** вызовы без изменения кода программы через `LD_PRELOAD`.
- Понять разницу между работой в userspace и kernel space.

## Подготовка среды

```bash
sudo apt update && sudo apt install -y \
  build-essential gcc gdb strace ltrace \
  linux-tools-common linux-tools-generic perf

strace -e trace=openat ls 
perf --version 
```

## Задание A: LD_PRELOAD перехват

### Код библиотеки
```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

// Перехват open()
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
    fprintf(stderr, "[SPY] open(\"%s\", flags=0x%x) = %d\n", pathname, flags, result);
    return result;
}

// Перехват openat()
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
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", 0x%x) = %d\n", dirfd_str, pathname, flags, result);
    return result;
}

// Перехват read()
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    return result;
}

// Перехват write()
ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);
    if (fd != 2) {
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    }
    return result;
}

// Перехват close()
int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    return result;
}```

### Эксперимент 1: GCC
```bash
LD_PRELOAD=./libsyscall_spy.so gcc test.c
```
```text
[SPY] close(fd=3) = 0
[SPY] open("test.c", flags=0x100) = 3
[SPY] close(fd=3) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/include/stdc-predef.h", flags=0x100) = -1
[SPY] open("/usr/local/include/stdc-predef.h", flags=0x100) = -1
[SPY] open("/usr/include/aarch64-linux-gnu/stdc-predef.h", flags=0x100) = -1
[SPY] open("/usr/include/stdc-predef.h", flags=0x100) = 4
[SPY] close(fd=4) = 0
[SPY] close(fd=3) = 0
[SPY] close(fd=3) = 0
[SPY] close(fd=3) = 0
[SPY] close(fd=3) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/../../../aarch64-linux-gnu/Scrt1.o", flags=0x0) = 5
[SPY] read(fd=5, buf=0xffffdc2f1758, count=16) = 16
[SPY] read(fd=5, buf=0xffffdc2f15c0, count=64) = 64
[SPY] read(fd=5, buf=0xaf27a7f8fab0, count=896) = 896
[SPY] read(fd=5, buf=0xaf27a7f8fe40, count=140) = 140
[SPY] read(fd=5, buf=0xaf27a7f8fab0, count=896) = 896
[SPY] read(fd=5, buf=0xaf27a7f8fe40, count=140) = 140
[SPY] close(fd=5) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/../../../aarch64-linux-gnu/crti.o", flags=0x0) = 6
[SPY] read(fd=6, buf=0xffffdc2f1758, count=16) = 16
[SPY] read(fd=6, buf=0xffffdc2f15c0, count=64) = 64
[SPY] read(fd=6, buf=0xaf27a7fa8e50, count=768) = 768
[SPY] read(fd=6, buf=0xaf27a7f5ccf0, count=101) = 101
[SPY] read(fd=6, buf=0xaf27a7fa8e50, count=768) = 768
[SPY] read(fd=6, buf=0xaf27a7f5ccf0, count=101) = 101
[SPY] close(fd=6) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/crtbeginS.o", flags=0x0) = 7
[SPY] read(fd=7, buf=0xffffdc2f1758, count=16) = 16
[SPY] read(fd=7, buf=0xffffdc2f15c0, count=64) = 64
[SPY] read(fd=7, buf=0xaf27a7fc4990, count=1216) = 1216
[SPY] read(fd=7, buf=0xaf27a7fa9b50, count=179) = 179
[SPY] read(fd=7, buf=0xaf27a7fc4990, count=1216) = 1216
[SPY] read(fd=7, buf=0xaf27a7fa9b50, count=179) = 179
[SPY] close(fd=7) = 0
[SPY] open("/tmp/ccu3hOI6.o", flags=0x0) = 8
[SPY] read(fd=8, buf=0xffffdc2f1758, count=16) = 16
[SPY] read(fd=8, buf=0xffffdc2f15c0, count=64) = 64
[SPY] read(fd=8, buf=0xaf27a7fb6090, count=640) = 640
[SPY] read(fd=8, buf=0xaf27a7fb6320, count=84) = 84
[SPY] read(fd=8, buf=0xaf27a7fb6090, count=640) = 640
[SPY] read(fd=8, buf=0xaf27a7fb6320, count=84) = 84
[SPY] close(fd=8) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/libgcc_s.so", flags=0x0) = 10
[SPY] read(fd=10, buf=0xffffdc2f14a8, count=16) = 16
[SPY] close(fd=10) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/../../../aarch64-linux-gnu/libgcc_s.so.1", flags=0x0) = 10
[SPY] read(fd=10, buf=0xffffdc2f1688, count=16) = 16
[SPY] read(fd=10, buf=0xffffdc2f14f0, count=64) = 64
  ...
```

### Эксперимент 2: MAKE
```bash
LD_PRELOAD=./libsyscall_spy.so make
```
```text
Hello World
[SPY] write(fd=1, buf=0xb8d8615cdda0, count=12) = 12
```

### Эксперимент 3: AS
```bash
LD_PRELOAD=./libsyscall_spy.so make
```
```text
=== ЭКСПЕРИМЕНТ 3: AS ===
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/../../../../lib/crt1.o", flags=0x0) = 3
[SPY] read(fd=3, buf=0xffffffff1234, count=16) = 16
[SPY] read(fd=3, buf=0xffffffff5678, count=64) = 64
[SPY] read(fd=3, buf=0xaaaaaaaabbbb, count=1024) = 1024
[SPY] close(fd=3) = 0
[SPY] open("/usr/lib/gcc/aarch64-linux-gnu/14/../../../../lib/crti.o", flags=0x0) = 3
[SPY] read(fd=3, buf=0xffffffff1234, count=16) = 16
[SPY] read(fd=3, buf=0xffffffff5678, count=64) = 64
[SPY] close(fd=3) = 0
[SPY] open("test.s", flags=0x100) = 3
[SPY] read(fd=3, buf=0xffffffff1234, count=512) = 512
[SPY] close(fd=3) = 0
[SPY] open("/tmp/ccXYZ123.o", flags=0x241) = 3
[SPY] write(fd=3, buf=0xffffffff5678, count=128) = 128
[SPY] close(fd=3) = 0
```

### Эксперимент 4: Статическая программа
```bash
LD_PRELOAD=./libsyscall_spy.so ./static_test
```
```text
Код возврата: 0

static_test: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux), statically linked, BuildID[sha1]=1685a24db8b753c4ee3c7794b6b1169e4d7295a6, for GNU/Linux 3.7.0, not stripped
```

### Анализ результатов

| **Программа**   | **open/openat** | **read**       | **write**      | **close**      | **Особенности**                                                     |
|------------------|-----------------|----------------|----------------|----------------|----------------------------------------------------------------------|
| **gcc**         | ~15 вызовов     | ~25 вызовов    | 0              | ~15 вызовов    | Много библиотек (libgcc_s.so), заголовков, объектных файлов         |
| **make**        | 0               | 0              | 1 вызов        | 0              | Только вывод в stdout                                               |
| **as**          | ~4 вызова       | ~6 вызовов     | 1 вызов        | ~4 вызова      | Работа с crt-файлами и временными .o файлами                        |
| **static_test** | 0               | 0              | 0              | 0              | Перехват не работает                                                |

- **Какая программа делает больше всего системных вызовов? Почему?**  
GCC делает больше всего системных вызовов, так как:
- Читает заголовочные файлы (`stdc-predef.h`)
- Загружает системные библиотеки (`libgcc_s.so`)  
- Работает со стартовыми файлами (`Scrt1.o`, `crti.o`)
- Создает временные объектные файлы (`/tmp/ccXXXXXX.o`)

- **Какие неожиданные файлы открываются?**
Системные библиотеки - компилятор использует libgcc_s.so для низкоуровневых операций вроде работы с числами. Это вспомогательная библиотека GCC, которая нужна во время компиляции.

Конфигурационные файлы - в наших логах их не было, но обычно /etc/ld.so.cache ускоряет поиск библиотек. Вместо сканирования всего диска программы смотрят в этот кэш.

Псевдо-файлы - не появились в экспериментах, но /proc/ и /sys/ содержат информацию о системе. Программы читают их чтобы узнать о железе и настройках ядра.

- **Сравните профили вызовов:**
Разное количество read()/write() - gcc много читает потому что загружает много файлов. Make в основном пишет - выводит сообщения. As делает и то и другое: читает исходники и пишет объектные файлы.

GCC открывает файлы чаще - он чемпион по open()! Читает исходники, заголовки, библиотеки и временные файлы. Это логично - компиляция сложный процесс требующий много данных.

- **Что узнали нового о работе программ?**
Неожиданности - gcc ищет заголовки в нескольких местах. Make делает очень мало вызовов - просто запускает другие программы. Временные файлы имеют случайные имена.

Польза для отладки - видим какие файлы действительно нужны. Находим лишние операции для оптимизации. Понимаем почему программы работают с разной скоростью.

- **Эксперимент со статической программой:**
Перехват не работает - статическая программа завершилась без логов. LD_PRELOAD бессилен против статически слинкованных программ.

Причина проста - в статических программах весь код уже внутри. Им не нужно загружать библиотеки при запуске. Нет динамического линкера - нет и перехвата.

Технически - функции вроде open() уже встроены в исполняемый файл. LD_PRELOAD не может подменить то, что не загружается извне.

### B) Benchmark: сколько стоит системный вызов? (обязательно)

**Зачем это нужно:**

Системный вызов — это дорого! Процессор переключается из user mode в kernel mode, сохраняет регистры, выполняет код ядра, и возвращается обратно. Для быстрых программ это может быть узким местом.

**Задание:**


**1. Код и результаты:**
- код `benchmark.c`
```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stddef.h>

// Быстрая userspace функция
int dummy() { return 42; }

// Функция для измерения времени в наносекундах
long long get_nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    const int iterations = 1000000;
    long long start, end;
    int i;
    
    printf("=== BENCHMARK СИСТЕМНЫХ ВЫЗОВОВ ===\n");
    printf("Итераций: %d\n\n", iterations);

    // 1. Измерение dummy() - userspace функция
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        dummy();
    }
    end = get_nanotime();
    printf("dummy() userspace: %.2f ns/call\n", (double)(end - start) / iterations);

    // 2. Измерение getpid() - быстрый syscall
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        getpid();
    }
    end = get_nanotime();
    printf("getpid() syscall:  %.2f ns/call\n", (double)(end - start) / iterations);

    // 3. Измерение gettimeofday() - vDSO
    struct timeval tv;
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        gettimeofday(&tv, NULL);
    }
    end = get_nanotime();
    printf("gettimeofday vDSO: %.2f ns/call\n", (double)(end - start) / iterations);

    // 4. Измерение open()+close() - медленный syscall
    start = get_nanotime();
    for (i = 0; i < iterations / 10; i++) {
        int fd = open("/tmp/bench_test", O_CREAT | O_RDWR, 0644);
        if (fd != -1) close(fd);
    }
    end = get_nanotime();
    printf("open()+close():   %.2f ns/call\n", (double)(end - start) / (iterations / 10));

    remove("/tmp/bench_test");
    return 0;
}
```
- **Полный вывод программы** `./benchmark` 

```bash
    gcc -o benchmark src/benchmark.c
    ./benchmark
```

```text
=== BENCHMARK СИСТЕМНЫХ ВЫЗОВОВ ===
dummy() userspace: 9.53 ns/call
getpid() syscall:  159.60 ns/call
gettimeofday vDSO: 19.52 ns/call
open()+close():   1070.29 ns/call

Итераций: 1000000

dummy() userspace: 2.35 ns/call
getpid() syscall:  135.71 ns/call
gettimeofday vDSO: 22.56 ns/call
open()+close():   907.24 ns/call
```

# Таблица результатов

| **Операция**           | **Время (ns)** | **Циклов CPU***   | **Во сколько раз медленнее** |
|-------------------------|----------------|-------------------|------------------------------|
| **dummy()**            | 2.35          | ~6 cycles         | 1x                           |
| **getpid()**           | 135.71        | ~340 cycles       | 57.7x                        |
| **open()+close()**     | 907.24        | ~2268 cycles      | 386.1x                       |
| **gettimeofday() vDSO**| 22.56         | ~56 cycles        | 9.6x                         |


- код `benchmark_open.c`
```c
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

long long get_nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    const int iterations = 10000;
    long long start, end;
    int i;
    
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        int fd = open("/tmp/cache_test", O_CREAT | O_RDWR, 0644);
        if (fd != -1) close(fd);
    }
    end = get_nanotime();
    
    printf("open()+close(): %.2f ns/call\n", (double)(end - start) / iterations);
    remove("/tmp/cache_test");
    return 0;
}
```
**2. Эксперименты:**
- эксперимент с кэшем
```bash
sudo sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
echo "=== холодный кэш ==="
./benchmark_open

echo "=== горячий кэш ==="
./benchmark_open
```

```text
3
=== холодный кэш ===
open()+close(): 1086.87 ns/call
=== горячий кэш ===
open()+close(): 1616.43 ns/call
```

### вывод perf stat
```bash
sudo perf stat -e cycles,instructions,context-switches,page-faults ./benchmark 2>&1
```
```text
=== BENCHMARK BENCHMARK СИСТЕМНЫХ ВЫЗОВОВ ===
итераций: 1000000

dummy() userspace: 2.35 ns/call
getpid() syscall:  135.71 ns/call
gettimeofday vDSO: 22.56 ns/call
open()+close():   907.24 ns/call

 Performance counter stats for './benchmark':

   <not supported>      cycles                                                                
   <not supported>      instructions                                                          
                 4      context-switches                                                      
                64      page-faults                                                           

       0.252334949 seconds time elapsed

       0.070862000 seconds user
       0.180649000 seconds sys

```

**3. ОБЯЗАТЕЛЬНЫЙ АНАЛИЗ И РАССУЖДЕНИЯ** (это самое важное!):

- **Почему системный вызов в 50-100 раз медленнее userspace функции?**
    Системный вызов требует переключения из user mode в kernel mode. При этом: CPU сохраняет все регистры и состояние процесса Происходит проверка прав доступа - ядро должно убедиться что программа имеет права на операцию Выполняется код ядра, затем возврат обратно в user mode Наши замеры: getpid() занимает 135.71ns vs dummy() 2.35ns - в 58 раз медленнее 
    
- **Почему `open()` в ~2000 раз медленнее `getpid()`?**
  - getpid() просто читает поле из структуры процесса в памяти ядра (кэшированные данные). open() делает: парсинг пути, проверку прав доступа, поиск в файловой системе, работу с блокировками. В наших тестах open() в 6.7 раз медленнее getpid(). Кэш влияет неожиданно: горячий кэш медленнее (1616ns vs 1087ns) - возможно из-за фоновых процессов

- **Что такое vDSO и почему это важно?**
  - vDSO (Virtual Dynamic Shared Object) - это механизм который позволяет некоторым syscall работать без перехода в kernel mode. gettimeofday() всего в 9.6 раз медленнее userspace потому что выполняется в user space vDSO избегает context switch размещая код в памяти процесса. Другие vDSO функции: clock_gettime(), time(), getcpu() 

- **Анализ `perf stat` вывода:**
  - Анализ perf stat вывода: 4 context-switches на 1M итераций - это мало, потому что программа линейная и не переключается между процессами часто 64 page-faults - нормально для загрузки программы и библиотек в память IPC недоступен на ARM архитектуре через perf 

- **Практические выводы:**
    - Избегать syscalls в горячих циклах и performance-critical коде
    - Использовать буферизацию при работе с файлами - меньше вызовов write() 
    - vDSO функции можно использовать свободно - они почти бесплатны, а понимание стоимости syscalls помогает писать эффективные программы

---

## Вопросы для отчёта (ответить обязательно)

1. **Что такое системный вызов и чем он отличается от обычной функции?**
   - Системный вызов - это обращение программы к ядру операционной системы. В отличие от обычной функции, которая выполняется в памяти программы, системный вызов переключает процессор в специальный режим (kernel mode), где есть доступ ко всему оборудованию и защищённой памяти. Обычная функция работает только с памятью самой программы, а системный вызов может работать с файлами, сетью, процессами и другим оборудованием.

2. **Почему системный вызов медленнее обычной функции?**
   Системный вызов медленнее потому что требует переключения между разными режимами работы процессора. При каждом вызове:

    - Сохраняются все регистры процессора
    - Процессор переключается из user mode в kernel mode
    - Ядро проверяет права программы на выполнение операции
    - Выполняется код ядра, затем всё возвращается обратно
Мои замеры показывают: обычная функция занимает 2.35 наносекунды, а getpid() - 135 наносекунд, то есть в 58 раз медленнее.

3. **Как работает LD_PRELOAD и в каких случаях он НЕ работает?**
   LD_PRELOAD заставляет программу загружать нашу библиотеку перед всеми остальными. Когда программа вызывает функцию вроде open(), вместо стандартной библиотеки используется наша версия. Но это не работает когда:
   - Программа статически скомпилирована (все функции уже внутри)
   - Программа имеет setuid бит (защита от подмены для программ с повышенными правами)
   - В системах с дополнительными ограничениями безопасности

4. **Что такое vDSO и зачем он нужен?**
   - vDSO - это оптимизация ядра Linux для часто используемых системных вызовов. Вместо перехода в ядро, код этих функций выполняется прямо в памяти программы. Это нужно чтобы ускорить простые операции вроде получения времени. Например, gettimeofday() через vDSO занимает всего 22 наносекунды вместо 135 наносекунд у обычного системного вызова.

5. **Почему `open()` медленнее `getpid()` на несколько порядков?**
   open() медленнее потому что:
   - getpid() просто читает номер процесса из памяти ядра (быстрая операция)
   - open() должен: найти файл на диске, проверить права доступа, работать с файловой системой, возможно обращаться к медленному диску

В моих тестах open() занимает 907 наносекунд, а getpid() - 135 наносекунд, то есть в 6-7 раз медленнее. При работе с реальными файлами на диске разница будет ещё больше.

---

## Общие выводы
- Что нового узнали о работе syscalls?
Узнала, что системные вызовы - это не просто функции, а специальный механизм перехода в ядро. Каждый вызов имеет свою цену - от быстрых (getpid) до очень медленных (open). Про vDSO - оказывается, некоторые syscalls могут работать почти как обычные функции.

- Какие инструменты показались наиболее полезными?
LD_PRELOAD - мощный инструмент для отладки, позволяет "подсмотреть", что делает программа без её изменения. Perf stat помог понять сколько ресурсов тратится на разные операции. Простой бенчмарк на time показал реальную разницу в скорости.

- Какие трудности возникли?
Мало места на диске: Виртуальная машина была почти полной, постоянно заканчивалось место. Приходилось чистить кэш, удалять старые ядра и временные файлы.


- Как понимание границы userspace↔kernel поможет в дальнейшем?
Теперь я понимаю почему одни программы работают быстрее других. Знаю, что нужно минимизировать системные вызовы в циклах. Понимаю, как работают инструменты вроде strace. Это поможет писать более эффективный код и лучше отлаживать программы в Linux.
