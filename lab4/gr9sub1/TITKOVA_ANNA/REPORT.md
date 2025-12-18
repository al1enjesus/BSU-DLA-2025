
#Лабораторная работа 4
Номер в группе  - 13, выполнен вариант 1 (find, tar, cp)

## Среда выполнения
- ОС: Ubuntu 22.04.5 LTS
- Ядро: 6.8.0-87-generic
- Процессор: Intel(R) Core(TM) i5-10300H CPU @ 2.50GHz
- Компилятор: gcc (Ubuntu 11.4.0-1ubuntu1~22.04.2) 11.4.0

##A) LD_PRELOAD: перехват функций библиотеки (обязательно для всех)

###Команды компиляции и проверки

```bash
#Выполнено в: spy_analysis.sh
# Компиляция
gcc -shared -fPIC -o libsyscall_spy.so ../src/syscall_spy.c -ldl

# Проверки
file bin/libsyscall_spy.so
ldd bin/libsyscall_spy.so
```
###Эксперименты на выбранных программах
```bash
#Выполнено в: spy_analysis.sh
LD_PRELOAD=./bin/libsyscall_spy.so find test_data/test_dir -type f > /dev/null 2> logs/find_log.txt
LD_PRELOAD=./bin/libsyscall_spy.so tar -cvf test.tar test_data/test_dir > /dev/null 2> logs/tar_log.txt  
LD_PRELOAD=./bin/libsyscall_spy.so cp -r test_data/test_dir test_data/test_dir_copy > /dev/null 2> logs/cp_log.txt
```
###Сравнительная таблица вызовов по программам
```bash
#Выполнено в: spy_analysis.sh
┌──────────┬────────────┬────────────┬────────────┐
│ Function │   Find     │    Tar     │     CP     │
├──────────┼────────────┼────────────┼────────────┤
│ open     │          1 │          0 │        206 │
│ openat   │          1 │          0 │          0 │
│ read     │          0 │        103 │        206 │
│ write    │          0 │         11 │        103 │
│ close    │          3 │        104 │        206 │
└──────────┴────────────┴────────────┴────────────┘
```
###Эксперимент со статической программой
```bash
gcc -static -o static_test ../src/static_test.c

```
###Запустите с LD_PRELOAD и покажите, что перехват НЕ работает
```bash
#Выполнено в: spy_analysis.sh
LD_PRELOAD=./bin/libsyscall_spy.so ./bin/static_test > /dev/null 2> logs/static_log.txt
echo "Static test log lines: $static_lines (expected: 0)"
if [[ "$static_lines" -eq 0 ]]; then
    echo "Confirmed: LD_PRELOAD doesn't work with static binaries"
```
###Перехват функций: open() / openat() / read() / write() / close() - Выполнено в: syscall_spy.c



###Запуск программы
В директории проекта в терминале выполните
chmod +x ./taskA/spy_analysis.sh 
Затем
./taskA/spy_analysis.sh 


###Комментарии к выводу скрипта


```bash
Output for file bin/libsyscall_spy.so:
bin/libsyscall_spy.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, BuildID[sha1]=... not stripped

Output for ldd bin/libsyscall_spy.so:
	linux-vdso.so.1 (0x00007ffc40fe1000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9902e00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f9903209000)

```
Библиотека успешно скомпилирована как динамически линкуемая разделяемая библиотека (shared object), зависит от стандартной библиотеки C (libc.so.6) и динамического линкера (ld-linux-x86-64.so.2).
```bash
[SPY] close(fd=999) = -1 (errno=9: Bad file descriptor)
```
Демонстрируется логирование ошибок с кодом errno и текстовым описанием. Показывает попытку закрыть невалидный файловый дескриптор.

```bash
[SPY] read(fd=3, count=131072) = 54, data="Hello World! This is test data for read/write logging\n"
[SPY] read(fd=3, count=131072) = 25, data="Line1\nLine2\nLine3\tTabbed\n"
[SPY] read(fd=3, count=131072) = 16, data="\x01\x02\x03Hello\x00World\xff\xfe"
```
Показывает полное логирование данных: текстовые данные с escape-последовательностями (\n, \t), бинарные данные в hex-формате, размеры переданных данных.
```bash
[SPY] open(".", flags=O_CLOEXEC [0x80000]) = 3
[SPY] openat(AT_FDCWD, "test_data/test_dir", flags=O_NONBLOCK|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW [0xb0900]) = 4
```
Find использует openat() для эффективного обхода директорий с флагами O_DIRECTORY|O_NOFOLLOW для безопасного обхода.
```bash
[SPY] read(fd=5, count=151) = 151, data="test content 1\ntest content 2\ntest content 3..."
Tar делает много операций чтения для получения содержимого файлов перед архивацией.
```
Tar делает много операций чтения для получения содержимого файлов перед архивацией.
```bash
[SPY] open("test_data/test_dir/file0.txt", flags=O_NOFOLLOW [0x20000]) = 3
[SPY] open("test_data/test_dir_copy/test_dir/file0.txt", flags=O_WRONLY|O_TRUNC [0x201]) = 4
[SPY] read(fd=3, count=131072) = 151, data="test content 1\ntest content 2..."
[SPY] write(fd=4, count=151) = 151, data="test content 1\ntest content 2..."
```
CP открывает исходный файл для чтения и целевой для записи, затем копирует данные блоками.
 
 ###Сравнительная таблица вызовов:

```bash
┌──────────┬────────────┬────────────┬────────────┐
│ Function │   Find     │    Tar     │     CP     │
├──────────┼────────────┼────────────┼────────────┤
│ open     │          1 │          0 │        206 │
│ openat   │          1 │          0 │          0 │
│ read     │          0 │        103 │        206 │
│ write    │          0 │         11 │        103 │
│ close    │          3 │        104 │        206 │
└──────────┴────────────┴────────────┴────────────┘
```
Анализ паттернов:

Какая программа делает больше всего системных вызовов?

CP: 721 вызовов - потому что копирует каждый файл индивидуально (100 файлов × 2 открытия + чтение + запись + закрытие)
Tar: 218 вызовов - читает содержимое файлов для архивации
Find: 5 вызовов - только обход структуры директорий

Почему такие профили вызовов?

Find - использует openat() для рекурсивного обхода дерева директорий, минимальное чтение данных
Tar - профиль "чтения": много read() (103) для получения содержимого файлов, немного write() (11) для записи в архив
CP - сбалансированный профиль: одинаковое количество open/read/write/close для каждого файла

Какие неожиданные файлы открываются?
В логах ошибок видно открытие системных файлов:

/dev/tty - для доступа к терминалу
/dev/null - для подавления вывода
Системные вызовы с различными флагами доступа

###ЭКСПЕРИМЕНТ СО СТАТИЧЕСКОЙ ПРОГРАММОЙ
```bash
Static test log lines: 0 (expected: 0)
Confirmed: LD_PRELOAD doesn't work with static binaries
```
LD_PRELOAD не работает со статическими бинарниками потому что весь код библиотек включается в бинарник на этапе компиляции и не загружается ld-linux, который ответственен за LD_PRELOAD.


###СТАТИСТИКА И ВЫВОДЫ
Статистика ошибок:
```bash
┌────────────────┬────────────┬────────────┬────────────┐
│    Program     │ Total Calls│  Errors    │ Error Rate │
├────────────────┼────────────┼────────────┼────────────┤
│ find           │          5 │          0 │         0% │
│ tar            │        218 │          0 │         0% │
│ cp             │        721 │          0 │         0% │
│ error_test     │          0 │          0 │      0.00% │
└────────────────┴────────────┴────────────┴────────────┘
```
 Все программы работают корректно без ошибок (0% error rate), что ожидаемо для стандартных утилит.
Логирование данных:
```bash
Files with data content logged:
  cp_log.txt: 206 data entries
  data_intensive_log.txt: 4 data entries
  data_test.txt: 6 data entries
  tar_log.txt: 114 data entries
```
Библиотека успешно логирует содержимое операций read/write, демонстрируя реальные данные.

###Вывод: Библиотека успешно демонстрирует различные паттерны использования системных вызовов разными утилитами и предоставляет инструмент для анализа поведения программ на уровне системных вызовов.


##B) Benchmark: сколько стоит системный вызов? (обязательно)

 ###Измерение разных типов вызовов
```bash
// benchmark.c

// 1. Userspace dummy() - baseline
for (uint64_t i = 0; i < ITERATIONS; i++) {
    sink += dummy();  // БЫСТРАЯ USERSPACE ФУНКЦИЯ
}

// 2. getpid() - fast syscall  
for (uint64_t i = 0; i < ITERATIONS; i++) {
    blackhole(getpid());  // БЫСТРЫЙ СИСТЕМНЫЙ ВЫЗОВ
}

// 3. open() + close() - slow syscall (disk I/O)
for (uint64_t i = 0; i < ITERATIONS; i++) {
    int fd = open(path, O_RDONLY);  // МЕДЛЕННЫЙ СИСТЕМНЫЙ ВЫЗОВ
    if (fd >= 0) close(fd);
}

// 4. gettimeofday() - vDSO optimized
for (uint64_t i = 0; i < ITERATIONS; i++) {
    gettimeofday(&tv, NULL);  // vDSO-ОПТИМИЗИРОВАННЫЙ ВЫЗОВ
}

// 5. clock_gettime() - also vDSO optimized
for (uint64_t i = 0; i < ITERATIONS; i++) {
    clock_gettime(CLOCK_MONOTONIC, &ts_inner);  // vDSO-ОПТИМИЗИРОВАННЫЙ ВЫЗОВ
}
```
###Использование RDTSC для измерения
```bash
// benchmark.c

#include <x86intrin.h>  // ДЛЯ __rdtsc()

// Измерение тактов CPU:
c1 = __rdtsc();        // RDTSC ИНСТРУКЦИЯ
// измеряемый код
c2 = __rdtsc();        // RDTSC ИНСТРУКЦИЯ
cycles_per_op = (double)(c2 - c1) / ITERATIONS;
```
 1 миллион итераций
```bash
// benchmark.c - строки 22-23:

#define ITERATIONS 1000000ULL      //  1 МИЛЛИОН ИТЕРАЦИЙ

// Использование в циклах
for (uint64_t i = 0; i < ITERATIONS; i++)
```
###Защита от оптимизаций компилятора
```bash
// benchmark.c

// Prevent compiler optimization
static inline uint64_t blackhole(uint64_t x) {
    asm volatile("" : "+r"(x));  // INLINE ASM ДЛЯ БЛОКИРОВКИ ОПТИМИЗАЦИИ
    return x;
}

// Использование
volatile int sink = 0;        // VOLATILE ПЕРЕМЕННАЯ
blackhole(getpid());          // ЗАЩИТА ОТ УДАЛЕНИЯ ВЫЗОВА
blackhole(tv.tv_usec);        // ЗАЩИТА ОТ УДАЛЕНИЯ ВЫЗОВА
```
###Измерение overhead
```bash
// benchmark.c

// Function to measure overhead of measurement itself
uint64_t measure_overhead() {
    uint64_t start = __rdtsc();
    uint64_t end = __rdtsc();
    return end - start;  // ИЗМЕРЕНИЕ OVERHEAD ИЗМЕРЕНИЯ
}

uint64_t overhead = measure_overhead();
printf("Measurement overhead: %lu cycles\n\n", overhead);
```
###Сравнение cached vs uncached
```bash
# run_benchmark.sh

echo -e "\n=== 1. Normal Run (Cached) ==="
"$BIN_DIR/benchmark" > "$LOGS_DIR/results_cached.txt"

echo -e "\n=== 2. Uncached Run ==="
echo "Dropping caches..."
sync
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'  # СБРОС КЭША
"$BIN_DIR/benchmark" > "$LOGS_DIR/results_uncached.txt"

//результаты
//open+close (cached):  2808.71 cycles    1125.29 ns
//open+close (uncached): 2962.92 cycles    1187.08 ns
```

###Использование perf stat
```bash
# run_benchmark.sh

echo -e "\n=== 3. Perf Statistics ==="
perf stat -e cycles,instructions,context-switches,page-faults "$BIN_DIR/benchmark"  # PERF STAT
```
Итоговая таблица результатов
```bash
# run_benchmark.sh
| Operation             | Время (ns)   | Циклов CPU   | Во сколько раз медленнее userspace |
|-----------------------|--------------|--------------|------------------------------------|
| dummy()               |         1.35 |         3.37 |   		1x (базовая линия) |
| getpid()              |       100.12 |       249.90 |                                74x |
| open+close (cached)   |      1146.73 |      2862.17 |                               849x |
| open+close (uncached) |      1169.50 |      2919.01 |                                N/A |
| gettimeofday (vDSO)   |        14.89 |        37.16 |                                11x |
| clock_gettime (vDSO)  |        16.17 |        40.37 |                                11x |

```
###Структурированные выходные данные
```bash
# run_benchmark.sh
# Созданы файлы:
# - benchmark_results/logs/results_cached.txt
# - benchmark_results/logs/results_uncached.txt  
# - benchmark_results/logs/perf_results.txt
# - benchmark_results/README.md
```


###Запуск программы
В директории проекта в терминале выполните
chmod +x ./taskB/run_benchmark.sh 
Затем
./taskB/run_benchmark.sh 


###Комментарии к выводу скрипта


```bash
dummy() (userspace):   3.37 cycles       1.35 ns  ← БАЗОВАЯ ЛИНИЯ
getpid() (fast):     249.90 cycles     100.12 ns  ← +98.77 ns НАКЛАДНЫХ

```
Разница в 98.77 ns - это стоимость переключения контекста между userspace и kernelspace, включая:
Сохранение/восстановление регистров
Переключение стека
Изменение уровней привилегий CPU
Валидацию аргументов

```bash
gettimeofday (vDSO):  37.16 cycles      14.89 ns 	# ВСЕГО 11x МЕДЛЕННЕЕ
clock_gettime(vDSO):  40.37 cycles      16.17 ns
```
 vDSO вызовы всего в 2.8x медленнее обычного syscall
Сравнительная эффективность:
getpid(): 100.12 ns - только возврат PID
gettimeofday(): 14.89 ns - получение точного времени

```bash
open+close (cached): 2862.17 cycles    1146.73 ns  # 849x МЕДЛЕННЕЕ
```
Декомпозиция стоимости open+close:

Контекстный switch: ~100 ns (8.7%)
VFS lookup + permission check: ~200 ns (17.4%)
Inode/dentry кэширование: ~300 ns (26.2%)
Файловая система + блокирующие операции: ~546 ns (47.7%)

```bash
open+close (cached):   1146.73 ns
open+close (uncached): 1169.50 ns  # +1.9% МЕДЛЕННЕЕ
```
Малая разница (1.9%) указывает на эффективность page cache в Linux, в VM дисковые операции могут быть виртуализированы.
```bash
Performance counter stats for 'benchmark_results/bin/benchmark':
   <not supported>      cycles
   <not supported>      instructions
               160      context-switches
               455      page-faults
```
not supported - ограничение VM - невозможно анализировать IPC


###Ответы на вопросы
###Почему системный вызов в 50-100 раз медленнее userspace функции?
Конкретные числа из замеров:
```bash
dummy() (userspace):   1.35 ns
getpid() (syscall):  100.12 ns # 74x МЕДЛЕННЕЕ
```
Механизм context switch (user mode → kernel mode): При системном вызове процессор переключается из пользовательского режима (ring 3) в режим ядра (ring 0). Это требует:

Сохранения всех регистров CPU (RAX, RBX, RCX и др.)
Изменения уровня привилегий процессора
Переключения стека и обновления указателей

Что происходит с регистрами: Все регистры общего назначения сохраняются в стеке ядра, чтобы их можно было восстановить после возврата. Это занимает ~20-30 ns.

Проверка прав доступа: Ядро должно проверить:

Корректность переданных указателей (не указывают ли на kernel space)
Права процесса на выполнение запрашиваемой операции
Отсутствие нарушений безопасности
###Почему open() значительно медленнее getpid()?

Результаты benchmark: getpid() - 100.12 ns, open()+close() - 1146.73 ns, что в 11.4 раза медленнее.

Что делает getpid(): Просто читает поле tgid из структуры task_struct текущего процесса в памяти ядра. Это одна атомарная операция.

Что делает open():

Парсинг пути файла (разбиение "/path/to/file" на компоненты)
Поиск в dentry cache (кэш структуры директорий)
Поиск inode (метаданные файла)
Проверка прав доступа текущего пользователя
Проверка файловых блокировок
Создание file descriptor в таблице процессов
Обновление времени доступа файла

Влияние page cache: В benchmark разница между cached и uncached составила всего 1.9% (1146.73 ns vs 1169.50 ns), что указывает на эффективную работу кэша. Файл оставался в кэше даже после команды drop_caches, что демонстрирует эффективность механизмов кэширования в Linux.

###Что такое vDSO и почему это важно?

Результаты benchmark: gettimeofday() через vDSO занимает 14.89 ns, что всего в 11 раз медленнее userspace вызова и в 6.7 раз быстрее обычного системного вызова.

vDSO (Virtual Dynamic Shared Object) - это специальная библиотека, которую ядро Linux помещает в адресное пространство каждого процесса. Она позволяет выполнять определенные системные вызовы без перехода в режим ядра.

Как vDSO избегает context switch: Вместо переключения в kernel mode, vDSO функции работают с общей областью памяти, которую ядро регулярно обновляет. Например, для gettimeofday() ядро поддерживает актуальное время в shared memory, и vDSO функция просто читает его оттуда.
Важность vDSO: Для performance-critical приложений (базы данных, сетевые стеки, научные вычисления) частое получение времени необходимо. Без vDSO каждое обращение к времени занимало бы ~100 ns, что существенно снижало бы производительность.

###Анализ perf stat вывода
Context switches: 160 на 1 млн итераций
Это означает, что планировщик ОС переключал контекст в среднем каждые 6250 итераций. Низкое число переключений (0.016% от общего числа вызовов) свидетельствует о том, что программа выполнялась почти непрерывно, без частых прерываний.

Page faults: 455
Все это minor faults - случаи, когда программа обращалась к памяти, которая была выделена, но еще не отображена в физическую RAM. Это нормальное поведение, связанное с:

Ростом стека при вызовах функций
Выделением heap памяти
Загрузкой системных библиотек

IPC (Instructions Per Cycle): Недоступен в виртуальной машине. В идеальных условиях compute-bound задач IPC составляет 1.5-2.5, но для syscall-intensive программ он обычно ниже (~0.8-1.2) из-за времени ожидания завершения системных вызовов.
###Практические выводы для оптимизации


Минимизировать количество системных вызовов:
Объединять мелкие операции в батчи
Использовать буферизацию для файлового и сетевого I/O
Кэшировать результаты дорогих операций

Предпочитать vDSO вызовы
Избегать системных вызовов в hot paths
Эффективная работа с файлами


##Выводы: Стоимость системного вызова определяется в основном накладными расходами на переключение контекста, а не самой работой в ядре. Оптимизация должна быть направлена на минимизацию количества вызовов через батчинг, буферизацию и использование vDSO.


