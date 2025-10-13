# Отчет по Лабораторной Работе №4: Системные вызовы

## 1. Цель работы

Целью данной лабораторной работы было изучение механизма взаимодействия программ с ядром Linux через системные вызовы (syscalls). Основные задачи включали:
*   Практическое освоение механизма перехвата библиотечных функций с помощью переменной окружения `LD_PRELOAD`.
*   Анализ и сравнение профилей системных вызовов различных утилит командной строки (`gcc`, `make`, `as`).
*   Измерение и анализ накладных расходов (overhead) системных вызовов разной сложности по сравнению с обычными вызовами функций.
*   Изучение оптимизации системных вызовов с помощью механизма vDSO.

## 2. Шаги и решения

### Шаг 1: Перехват вызовов с помощью LD_PRELOAD (Задание А)

**Решение:** Была создана разделяемая библиотека `libsyscall_spy.so`, которая перехватывает функции `open`, `openat`, `read`, `write` и `close`. При вызове любая из этих функций сначала логирует свои аргументы и результат в `stderr`, а затем передает управление оригинальной функции из `libc` с помощью `dlsym(RTLD_NEXT, ...)`.

**Код `src/syscall_spy.c`:**
```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

// Указатели на оригинальные функции
static int (*original_open)(const char*, int, ...) = NULL;
static int (*original_openat)(int, const char*, int, ...) = NULL;
static ssize_t (*original_read)(int, void*, size_t) = NULL;
static ssize_t (*original_write)(int, const void*, size_t) = NULL;
static int (*original_close)(int) = NULL;

// Макрос для инициализации указателя на оригинальную функцию
#define INIT_ORIGINAL_FUNC(name) \
    if (!original_##name) { \
        original_##name = dlsym(RTLD_NEXT, #name); \
    }

// Перехват open()
int open(const char *pathname, int flags, ...) {
    INIT_ORIGINAL_FUNC(open);

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
    INIT_ORIGINAL_FUNC(openat);

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
    INIT_ORIGINAL_FUNC(read);
    ssize_t result = original_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    return result;
}

// Перехват write()
ssize_t write(int fd, const void *buf, size_t count) {
    INIT_ORIGINAL_FUNC(write);
    ssize_t result = original_write(fd, buf, count);
    if (fd != 2) { // Избегаем рекурсии, не логируя запись в stderr
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    }
    return result;
}

// Перехват close()
int close(int fd) {
    INIT_ORIGINAL_FUNC(close);
    int result = original_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    return result;
}
```
**Компиляция библиотеки:**
```bash
gcc -shared -fPIC -o libsyscall_spy.so src/syscall_spy.c -ldl
```

**Проведение экспериментов:**
Библиотека была применена к трем программам из группы №3 (`gcc`, `make`, `as`).

1.  **`gcc`**: Была выполнена компиляция простого файла `hello.c`.
    ```bash
    printf '#include <stdio.h>\nint main() { printf("Hello GCC!\\n"); return 0; }' > hello.c
    LD_PRELOAD=./libsyscall_spy.so gcc hello.c -o hello 2> logs/gcc_log.txt
    ```
2.  **`make`**: Был выполнен простой `Makefile`, выводящий строку на экран.
    ```bash
    printf 'all:\n\t@echo "Make is running!"\n' > Makefile
    LD_PRELOAD=./libsyscall_spy.so make 2> logs/make_log.txt
    ```
3.  **`as`**: Был ассемблирован `.s` файл, полученный из `hello.c`.
    ```bash
    gcc -S hello.c -o hello.s
    LD_PRELOAD=./libsyscall_spy.so as hello.s -o hello.o 2> logs/as_log.txt
    ```

### Шаг 2: Измерение производительности системных вызовов (Задание B)

**Решение:** Была написана программа `benchmark.c`, измеряющая время выполнения четырех операций в цикле из 1 млн итераций. Для получения точных замеров использовалась инструкция `__rdtsc`.

```c
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define ITERATIONS 1000000
#define CPU_FREQ_GHZ 2.7

// 1. Быстрая функция в userspace
__attribute__((noinline))
int dummy_func() {
    return 42;
}

// 2. Быстрый системный вызов
__attribute__((noinline))
pid_t getpid_call() {
    return getpid();
}

// 3. Медленный системный вызов
__attribute__((noinline))
int open_close_call() {
    int fd = open("/tmp/testfile", O_WRONLY | O_CREAT, 0644);
    if (fd == -1) return -1;
    return close(fd);
}

// 4. vDSO-вызов
__attribute__((noinline))
int gettimeofday_call(struct timeval *tv) {
    return gettimeofday(tv, NULL);
}

int main() {
    uint64_t start, end;
    uint64_t total_cycles = 0;
    struct timeval tv;

    // --- Измерение dummy_func ---
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        dummy_func();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_dummy = (double)total_cycles / ITERATIONS;
    printf("1. Userspace dummy():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns\n",
           avg_dummy, avg_dummy / CPU_FREQ_GHZ);

    // --- Измерение getpid() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        getpid_call();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_getpid = (double)total_cycles / ITERATIONS;
    printf("2. Syscall getpid():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_getpid, avg_getpid / CPU_FREQ_GHZ, avg_getpid / avg_dummy);

    // --- Измерение open()+close() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        open_close_call();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_open_close = (double)total_cycles / ITERATIONS;
    printf("3. Syscall open()+close():\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_open_close, avg_open_close / CPU_FREQ_GHZ, avg_open_close / avg_dummy);

    // --- Измерение gettimeofday() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        gettimeofday_call(&tv);
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_gettimeofday = (double)total_cycles / ITERATIONS;
    printf("4. vDSO gettimeofday():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_gettimeofday, avg_gettimeofday / CPU_FREQ_GHZ, avg_gettimeofday / avg_dummy);

    return 0;
}
```

**Проведение эксперимента:**
Программа была скомпилирована и запущена, ее вывод был записан в лог.
```bash
gcc -O2 -o benchmark src/benchmark.c
touch /tmp/testfile
./benchmark
```
Дополнительно были проведены замеры влияния кэша страниц и анализ с помощью `perf stat`.

## 3. Ключевые выводы

### Выводы по Заданию А

*   **Профили вызовов отражают сложность программ:** Утилита `as` показала минимальную I/O активность, соответствующую простой задаче "прочитать-записать". `gcc` продемонстрировал на порядок большую активность из-за необходимости поиска и чтения десятков заголовочных файлов. `make` показал максимальную активность, так как его вызовы суммируются с вызовами порождаемого им процесса `gcc`.
*   **LD_PRELOAD наследуется дочерними процессами:** Логи `make` содержали системные вызовы от `gcc`, что доказывает наследование переменной окружения.
*   **Механизм `LD_PRELOAD` не работает на статических программах:** Эксперимент со статически скомпонованной программой показал полное отсутствие логов. Это подтверждает, что `LD_PRELOAD` — это функция динамического компоновщика (`ld.so`), который не используется при запуске статических бинарных файлов.

### Выводы по Заданию B

*   **Системные вызовы имеют высокую стоимость:** Переход в режим ядра даже для простейшего вызова `getpid()` оказался почти **в 7 раз** дороже обычной функции. Это цена переключения контекста.
*   **Сложность операции в ядре имеет значение:** "Тяжелый" вызов `open()`+`close()` оказался почти **в 85 раз** медленнее базовой функции, что на порядок медленнее `getpid()`. Это демонстрирует стоимость сложной логики внутри ядра (работа с VFS, проверка прав).
*   **vDSO — эффективнейшая оптимизация:** Вызов `gettimeofday()` оказался всего в **1.06 раза** медленнее обычной функции, что наглядно показывает, как vDSO устраняет накладные расходы на переключение контекста, выполняя код в пространстве пользователя.
*   **Кэш страниц критически важен:** Эксперимент со сбросом кэша показал, что `open()` выполняется **в 3.4 раза** медленнее при "холодном" доступе, что подтверждает огромную роль кэша в производительности файловых операций.

## 4. Ответы на вопросы задания

1.  **Что такое системный вызов и чем он отличается от обычной функции?**
    Системный вызов — это запрос программы к ядру для выполнения привилегированной операции, исполняемый в `kernel mode`. Обычная функция исполняется в `user mode` с ограниченными правами.

2.  **Почему системный вызов медленнее обычной функции?**
    Из-за накладных расходов на переключение контекста. Как показали замеры в задании B, даже самый быстрый syscall (`getpid`) примерно **в 7 раз** медленнее вызова пустой функции.

3.  **Как работает LD_PRELOAD и в каких случаях он НЕ работает?**
    Это переменная окружения, заставляющая динамический компоновщик `ld.so` загрузить указанную библиотеку первой. Он не работает на статически скомпонованных программах (так как `ld.so` не используется) и на программах с `setuid`-битом (в целях безопасности).

4.  **Что такое vDSO и зачем он нужен?**
    Это механизм, при котором ядро отображает в пространство пользователя страницу с реализацией некоторых syscalls. Это позволяет вызывать их без переключения в режим ядра. Как показал бенчмарк, `gettimeofday()` через vDSO всего в **1.06 раза** медленнее userspace-функции.

5.  **Почему `open()` медленнее `getpid()` на несколько порядков?**
    `getpid()` просто читает значение из структуры в ядре. `open()` запускает сложный процесс: парсинг пути, проверка прав доступа, работа с файловой системой и, возможно, обращение к диску.

## 5. Как проверяли

*   **Корректность перехвата (Задание А):** Работа `libsyscall_spy.so` проверялась по наличию префикса `[SPY]` в логах. Для "тихой" утилиты `as` была намеренно вызвана ошибка, чтобы убедиться, что логгер работает и выводит данные в `stderr` при его использовании. Эксперимент со статической программой служил негативным тестом, подтверждающим теорию.
*   **Корректность бенчмарка (Задание B):** Результаты замеров были верифицированы с помощью внешнего инструмента `perf stat`, который подтвердил наличие системных событий (переключений контекста, ошибок страниц). Для проверки гипотезы о влиянии кэша был проведен изолированный эксперимент со сбросом page cache.

## 6. Как использовали AI

Искусственный интеллект (в лице языковой модели Gemini) использовался как ассистент на нескольких этапах работы:
*   **Отладка окружения:** Помощь в диагностике и решении проблем с системными репозиториями `apt` и установкой пакета `perf` для нестандартной версии ядра.
*   **Интерпретация результатов:** Консультации по анализу логов, например, объяснение пустого вывода утилиты `as` или расшифровка вывода `perf stat`.
*   **Структурирование и формулировка отчета:** Помощь в перефразировании выводов для большей ясности и точности, а также в реструктуризации финального отчета в соответствии с заданными требованиями.

AI выступал в роли "научного руководителя": он не выполнял эксперименты, но помогал в анализе полученных данных и оформлении результатов.
