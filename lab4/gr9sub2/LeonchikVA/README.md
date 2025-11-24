# Лабораторная работа 4 — Системные вызовы

**Студент:** Номер 23  
**Группа программ:** 23 % 4 = **3** → `gcc`, `make`, `as`  
**Выполненные задания:** A (LD_PRELOAD), B (Benchmark)

## Структура проекта

```
lab4/gr<группа>sub<подгруппа>/ФАМИЛИЯ_ИМЯ/
├── REPORT.MD              # Полный отчёт с анализом
├── README.md              # Этот файл (инструкции)
├── src/
│   ├── Makefile           # Сборка проекта
│   ├── syscall_spy.c      # Задание A: библиотека перехвата
│   ├── benchmark.c        # Задание B: измерение времени
│   └── (собранные файлы)
└── logs/
    ├── gcc_syscalls.log   # Логи экспериментов
    ├── make_syscalls.log
    ├── as_syscalls.log
    └── benchmark_results.txt
```

## Быстрый старт

### 1. Сборка всего проекта

```bash
cd src/
make
```

Это соберёт:
- `libsyscall_spy.so` — библиотека для задания A
- `benchmark` — программа для задания B

### 2. Проверка сборки

```bash
make test
```

Быстрая проверка работоспособности обоих компонентов.

## Задание A: Перехват системных вызовов (LD_PRELOAD)

### Запуск на gcc

```bash
cd src/
LD_PRELOAD=./libsyscall_spy.so gcc --version 2>&1 | tee ../logs/gcc_syscalls.log
```

### Запуск на make

```bash
cd src/
LD_PRELOAD=./libsyscall_spy.so make --version 2>&1 | tee ../logs/make_syscalls.log
```

### Запуск на as (assembler)

```bash
cd src/
LD_PRELOAD=./libsyscall_spy.so as --version 2>&1 | tee ../logs/as_syscalls.log
```

### Запуск всех экспериментов сразу

```bash
cd src/
make run-all-experiments
```

### Эксперимент со статической программой

```bash
# Создаём простую программу
cat > test_static.c << 'EOF'
#include <stdio.h>
int main() {
    printf("Hello from static binary!\n");
    return 0;
}
EOF

# Компилируем статически
gcc -static -o test_static test_static.c

# Проверяем, что программа статическая
file test_static
ldd test_static  # должно вывести "not a dynamic executable"

# Пытаемся перехватить (не сработает!)
LD_PRELOAD=./libsyscall_spy.so ./test_static
```

**Ожидаемый результат:** Программа запустится, но логи перехвата **не появятся**, потому что статически слинкованные программы не используют динамический линкер.

## Задание B: Benchmark системных вызовов

### Базовый запуск

```bash
cd src/
./benchmark
```

Программа выполнит 1 миллион итераций каждого теста и выведет:
- Общее время выполнения
- Среднее время на один вызов
- Количество циклов CPU (если доступен RDTSC)

### Сохранение результатов

```bash
cd src/
./benchmark | tee ../logs/benchmark_results.txt
```

### Эксперимент с кэшем страниц (требует root)

```bash
# Прогрев кэша
./benchmark > /dev/null

# Сброс кэша
sudo sync
echo 3 | sudo tee /proc/sys/vm/drop_caches

# Холодный кэш
./benchmark | tee ../logs/benchmark_cold_cache.txt

# Горячий кэш (сразу же ещё раз)
./benchmark | tee ../logs/benchmark_hot_cache.txt
```

### Анализ через perf stat

```bash
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

**Примечание:** Если `perf` недоступен, установите:
```bash
sudo apt install linux-tools-generic linux-tools-common
```

## Интерпретация результатов

### Задание A (LD_PRELOAD)

**Что искать в логах:**
- Какие файлы открывают программы (системные библиотеки, конфиги)
- Сколько системных вызовов каждого типа
- Неожиданные файлы в `/proc`, `/sys`, `/etc`

**Пример анализа для gcc:**
- Много `openat()` для библиотек компилятора
- Чтение `/proc/cpuinfo` (определение архитектуры)
- Открытие shared libraries (`.so` файлы)

### Задание B (Benchmark)

**Что искать:**
- `dummy()` (userspace) должна быть самой быстрой (~1-5 ns)
- `getpid()` в 50-200 раз медленнее (~100-500 ns)
- `open()+close()` в 1000-5000 раз медленнее (~5000-20000 ns)
- `gettimeofday()` (vDSO) всего в 5-20 раз медленнее (~10-50 ns)

**Почему:**
- Userspace функция не переключается в kernel mode
- `getpid()` делает быстрый syscall (просто читает PID из памяти ядра)
- `open()` работает с файловой системой, проверяет права, может обратиться к диску
- vDSO избегает context switch, выполняясь в userspace с данными из ядра

## Воспроизводимость

### Требования

- **ОС:** Linux (протестировано на Ubuntu 20.04/22.04)
- **Компилятор:** gcc 9.0+
- **Инструменты:** make, perf (опционально)

### Проверка окружения

```bash
# Версия ядра
uname -r

# Процессор
lscpu | grep "Model name"

# Компилятор
gcc --version

# perf
perf --version
```

## Очистка

```bash
cd src/
make clean
```

Удалит все скомпилированные файлы.

## Устранение проблем

### LD_PRELOAD не работает

**Проблема:** Логи не появляются при использовании `LD_PRELOAD`

**Решения:**
1. Проверьте, что программа динамически слинкована: `ldd <программа>`
2. Убедитесь, что путь к библиотеке правильный (используйте абсолютный путь или `./`)
3. Некоторые программы с setuid игнорируют LD_PRELOAD из соображений безопасности

### Benchmark показывает странные результаты

**Проблема:** Userspace функция медленнее, чем ожидалось

**Решения:**
1. Отключите frequency scaling: `sudo cpupower frequency-set --governor performance`
2. Закройте другие приложения
3. Запустите несколько раз и возьмите медианное значение

### perf не работает

**Проблема:** `perf: command not found` или ошибки доступа

**Решения:**
1. Установите: `sudo apt install linux-tools-generic`
2. Для WSL2: `perf` может требовать дополнительной настройки ядра
3. Попробуйте с sudo: `sudo perf stat ...`

## Дополнительные эксперименты

### 1. Трассировка реальной компиляции

```bash
# Создаём простую программу
echo 'int main() { return 0; }' > hello.c

# Трассируем компиляцию
LD_PRELOAD=./libsyscall_spy.so gcc hello.c -o hello 2>&1 | \
    grep -E '\[SPY\]' | tee ../logs/gcc_compile_syscalls.log

# Анализируем
grep -c 'openat' ../logs/gcc_compile_syscalls.log
```

### 2. Сравнение разных файловых систем

```bash
# На обычном диске (ext4)
./benchmark

# На tmpfs (в памяти)
mkdir -p /tmp/ramdisk
./benchmark  # убедитесь, что /tmp на tmpfs
```

### 3. Измерение накладных расходов LD_PRELOAD

```bash
# Без перехвата
time gcc --version > /dev/null

# С перехватом
time LD_PRELOAD=./libsyscall_spy.so gcc --version 2>&1 > /dev/null
```

## Контакты и вопросы

Для вопросов по воспроизводимости см. REPORT.MD (там подробный анализ всех экспериментов).

---

**Статус:** ✅ Задания A и B выполнены  
**Дата:** 2025-10-16  
**Автор:** Студент №23