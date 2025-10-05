# Лабораторная 4 - Системные вызовы

**Группа программ:** 11 % 4 = 3 → `gcc`, `make`, `as`

## Быстрый старт

```bash
# Перейти в директорию с исходниками
cd src/

# Собрать все программы
make all

# Запустить быстрые тесты
make test

# Запустить полные эксперименты для отчёта
make experiments
```

## Структура проекта

```
src/
├── Makefile              # Сборка всех программ
├── syscall_spy.c         # Задание A: LD_PRELOAD библиотека
├── benchmark.c           # Задание B: Бенчмарк syscalls
├── mytracer.c            # Задание C*: Упрощённый strace
├── syscall_names.h       # Имена системных вызовов
├── hello_static.c        # Тестовая статическая программа
└── libsyscall_spy.so     # Собранная библиотека (после make)

logs/
├── gcc_test.log          # Результаты тестов (после make experiments)
├── make_test.log
├── benchmark_results.log
└── tracer_true.log
```

## Задание A: LD_PRELOAD перехват

### Сборка и использование

```bash
# Собрать библиотеку
make libsyscall_spy.so

# Тестирование на программах группы 3 (gcc, make, as)
LD_PRELOAD=./libsyscall_spy.so gcc --version
LD_PRELOAD=./libsyscall_spy.so make --version  
LD_PRELOAD=./libsyscall_spy.so as --version

# Тест на простой программе
LD_PRELOAD=./libsyscall_spy.so /bin/echo "Hello"

# Проверка, что не работает на статической программе
make hello_static
LD_PRELOAD=./libsyscall_spy.so ./hello_static
```

### Что перехватывается

- `open()` - старый интерфейс открытия файлов
- `openat()` - новый интерфейс (с dirfd)
- `read()` - чтение данных
- `write()` - запись данных (кроме stderr)
- `close()` - закрытие дескрипторов

## Задание B: Бенчмарк системных вызовов

### Запуск

```bash
# Собрать и запустить
make benchmark
./benchmark

# Дополнительные измерения с perf
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark

# Тест влияния кэша (требует root)
sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
./benchmark
```

### Что измеряется

- Userspace функция `dummy()` (базовая линия)
- Быстрый syscall `getpid()`  
- Медленный syscall `open() + close()`
- vDSO вызов `gettimeofday()`

По 1 миллиону итераций каждого, измерение в наносекундах и циклах CPU.

## Задание C*: Упрощённый strace

### Запуск

```bash
# Собрать tracer
make mytracer

# Тестирование на простых программах
./mytracer /bin/true
./mytracer /bin/echo "Hello"
./mytracer /bin/ls

# Сравнение с настоящим strace
strace -c /bin/true
./mytracer /bin/true | wc -l
```

### Возможности

- Перехват всех системных вызовов через `ptrace()`
- Декодирование номеров syscalls в имена
- Показ аргументов для основных вызовов
- Подсчёт общего количества вызовов

## Воспроизведение экспериментов

### Для задания A (группа программ 3)

```bash
# gcc - компилятор C
LD_PRELOAD=./libsyscall_spy.so gcc --version 2>&1 | head -20

# make - утилита сборки  
LD_PRELOAD=./libsyscall_spy.so make --version 2>&1 | head -20

# as - ассемблер
LD_PRELOAD=./libsyscall_spy.so as --version 2>&1 | head -20

# Тест статической программы
LD_PRELOAD=./libsyscall_spy.so ./hello_static
```

### Для задания B

```bash
# Основной бенчмарк
./benchmark

# С профилированием
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

### Для задания C*

```bash
# Наш tracer
./mytracer /bin/true

# Сравнение со strace
strace -c /bin/true
```

## Системные требования

- Linux (x86_64)
- GCC с поддержкой `-shared -fPIC`
- Библиотека libdl (`-ldl`)
- perf tools (для задания B)
- Права на ptrace (для задания C*)

### Установка зависимостей (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential gcc gdb strace ltrace linux-tools-common linux-tools-generic perf
```

## Ожидаемые результаты

### Задание A
- gcc открывает множество системных библиотек и заголовочных файлов
- make открывает Makefile'ы и проверяет временные метки файлов  
- as работает с объектными файлами и таблицами символов
- Статическая программа НЕ перехватывается

### Задание B  
- Userspace функция: ~2-5 ns, ~5-15 циклов
- getpid(): ~50-200 ns, ~150-500 циклов  
- open+close: ~2000-10000 ns, ~5000-25000 циклов
- gettimeofday vDSO: ~10-50 ns, ~25-150 циклов

### Задание C*
- /bin/true делает ~15-30 системных вызовов
- Основные: execve, openat, fstat, mmap, close
- Замедление в ~10-50 раз по сравнению с обычным запуском

## Troubleshooting

### LD_PRELOAD не работает
- Проверьте, что программа динамически слинкована: `ldd <программа>`
- Проверьте права доступа к библиотеке: `ls -la libsyscall_spy.so`
- Убедитесь, что используете правильный путь: `LD_PRELOAD=./libsyscall_spy.so`

### ptrace не работает  
- Проверьте настройки безопасности: `cat /proc/sys/kernel/yama/ptrace_scope`
- Если значение 1, попробуйте: `echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`

### perf не работает
- В WSL2: `echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid`
- Установите символы ядра: `sudo apt install linux-tools-$(uname -r)`

## Полезные команды

```bash
# Проверка типа программы (статическая/динамическая)
file /bin/ls
ldd /bin/ls

# Просмотр системных вызовов
strace -e trace=openat ls
ltrace ls

# Профилирование
perf stat ls
perf record ls; perf report

# Информация о системе
uname -a
lscpu | grep "Model name"
cat /proc/version
```