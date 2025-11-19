# Лабораторная работа 4 — Системные вызовы

**Студент:** Черноокий   
**Номер в списке:** 16 → Программы: ls, cat, grep

## Структура проекта

```
├── src/                      # Исходный код
│   ├── syscall_spy.c        # LD_PRELOAD библиотека для перехвата
│   ├── benchmark.c          # Измерение производительности syscalls
│   ├── test_preload.c       # Тестовая программа
│   └── Makefile            # Сборка проекта
├── logs/                   # Логи экспериментов
├── REPORT.MD              # Подробный отчет с результатами
└── README.md             # Этот файл
```

## Быстрый старт

### 1. Сборка
```bash
cd src/
make clean
make all
```

### 2. Демо LD_PRELOAD (Задание A)
```bash
# Перехват вызовов cat
LD_PRELOAD=./libsyscall_spy.so cat /dev/null

# Перехват вызовов grep
echo -e "line1\ntest line\nline3" | LD_PRELOAD=./libsyscall_spy.so grep "test"

# Тест со статической программой (НЕ должен работать)
LD_PRELOAD=./libsyscall_spy.so ./hello_static
```

### 3. Бенчмарк производительности (Задание B)
```bash
./benchmark
```

## Основные результаты

### Задание A: LD_PRELOAD перехват
-  **cat**: Успешно перехватывает open, read, write, close
-  **grep**: Перехватывает openat, read, close  
-  **ls**: Обходит стандартные библиотечные функции
-  **Статические программы**: Корректно НЕ перехватываются

### Задание B: Производительность syscalls
| Операция | Время (ns) | Замедление vs userspace |
|----------|------------|-------------------------|
| dummy() userspace | 1.29 | 1x (baseline) |
| getpid() | 214.66 | 167x |
| open+close | 3285.09 | 2556x |
| gettimeofday vDSO | 23.56 | 18x |

## Воспроизведение экспериментов

### Создание тестовых файлов
```bash
echo "test content for cat" > /tmp/test_cat.txt
echo -e "line1\ntest line\nline3\nanother test" > /tmp/test_grep.txt
```

### Эксперименты с моими программами (ls, cat, grep)
```bash
# CAT - успешный перехват
LD_PRELOAD=./libsyscall_spy.so cat /tmp/test_cat.txt
# Ожидаемый вывод: [SPY] open(...), [SPY] read(...), [SPY] write(...), [SPY] close(...)

# GREP - успешный перехват  
LD_PRELOAD=./libsyscall_spy.so grep "test" /tmp/test_grep.txt
# Ожидаемый вывод: [SPY] openat(...), [SPY] read(...), [SPY] close(...)

# LS - перехват не работает (использует прямые syscalls)
LD_PRELOAD=./libsyscall_spy.so ls -la
# Ожидаемый вывод: обычный ls без логов перехвата
```

### Сравнение со strace
```bash
# Статистика syscalls
strace -c cat /tmp/test_cat.txt
strace -c grep "test" /tmp/test_grep.txt  
strace -c ls -la

# Детальная трассировка
strace -e trace=openat,open,read,write,close cat /tmp/test_cat.txt
```

## Ключевые выводы

1. **LD_PRELOAD** работает только с динамически слинкованными программами, использующими стандартные библиотечные функции

2. **Современные утилиты** (ls) часто обходят libc и используют прямые системные вызовы для оптимизации

3. **Стоимость syscalls** огромна: простой getpid() в 167 раз медленнее обычной функции, open() в 2556 раз

4. **vDSO** — важная оптимизация ядра, позволяющая избежать дорогого context switch

5. **Файловые операции** значительно дороже простых syscalls из-за сложности файловых систем

## Требования

- Linux/WSL2 с gcc
- Библиотека libdl (обычно установлена по умолчанию)
- Права на создание файлов в /tmp

## Структура Makefile

```bash
make all          # Собрать все компоненты
make clean        # Очистить собранные файлы
make test-preload # Быстрый тест LD_PRELOAD
make test-benchmark # Быстрый тест бенчмарка
make test         # Полный набор тестов
```

Подробные результаты и анализ смотрите в **REPORT.MD**.