````markdown
# Лабораторная 4 — Системные вызовы

## Выбор программ
**Номер в списке группы:** 6  
**Расчёт группы программ:** 6 % 4 = 2  
**Программы для анализа:** `curl`, `wget` (сетевые программы)  
**Мои задания:** A (LD_PRELOAD), B (Benchmark) — обязательные

## Среда выполнения
- **ОС:** Linux (Ubuntu/Debian-based distribution)
- **Ядро:** Linux kernel (современная версия с поддержкой vDSO)
- **Процессор:** x86_64 архитектура с поддержкой RDTSC
- **Компилятор:** GCC с поддержкой C11/C++17

---

## Задание A: LD_PRELOAD перехват

### Код библиотеки

Создана библиотека `libsyscall_spy.so`, которая перехватывает:
- `open()`, `openat()`, `open64()`, `openat64()` — операции открытия файлов
- `read()`, `write()` — операции чтения и записи
- `close()` — закрытие файловых дескрипторов

**Особенности реализации:**
- Используется `dlsym(RTLD_NEXT, ...)` для вызова оригинальной функции
- Реализовано декодирование флагов open() (O_RDONLY, O_CREAT, O_CLOEXEC)
- Отображается errno с текстовым описанием
- Есть защита от рекурсии при логировании в stderr

### Эксперимент 1: curl (загрузка JSON API)

**Команда:**
```bash
LD_PRELOAD=./libsyscall_spy.so curl -sS -o dota_json.txt "https://store.steampowered.com/api/appdetails?appids=570&l=en"
````

**Вывод:**

```
[SPY] ═══════════════════════════════════════════════
[SPY] Syscall spy library loaded (LD_PRELOAD)
[SPY] Intercepting: open, openat, read, write, close
[SPY] ═══════════════════════════════════════════════

[SPY] open("/proc/sys/crypto/fips_enabled", flags=O_RDONLY (0x0)) = -1 [errno=2: No such file or directory]
[SPY] open("/home/mkv/.curlrc", flags=O_RDONLY (0x0)) = -1 [errno=2: Нет такого файла или каталога]
[SPY] open("/home/mkv/.config/curlrc", flags=O_RDONLY (0x0)) = -1 [errno=2: Нет такого файла или каталога]
[SPY] open("/home/mkv/.curlrc", flags=O_RDONLY (0x0)) = -1 [errno=2: Нет такого файла или каталога]
[SPY] close(fd=5) = 0
[SPY] write(fd=6, buf=0x7331093fec5b, count=1) = 1
[SPY] close(fd=6) = 0
[SPY] close(fd=5) = 0
[SPY] close(fd=3) = 0
[SPY] close(fd=4) = 0
[SPY] close(fd=5) = 0

[SPY] ═══════════════════════════════════════════════
[SPY] Syscall spy library unloaded
[SPY] ═══════════════════════════════════════════════
```

**Анализ:**

* **Количество вызовов:** около 15 (4 open, 1 write, 6 close)
* **Открываемые файлы:**

  * `/proc/sys/crypto/fips_enabled` — проверка FIPS-режима криптографии
  * `~/.curlrc`, `~/.config/curlrc` — попытка загрузить конфигурацию
* **Особенности:** минимальное количество системных вызовов благодаря буферизации libcurl

### Эксперимент 2: wget (загрузка HTML страницы)

**Команда:**

```bash
LD_PRELOAD=./libsyscall_spy.so wget -q -O steam_dota.html "https://store.steampowered.com/app/570/?l=english"
```

**Вывод (фрагмент):**

```
[SPY] ═══════════════════════════════════════════════
[SPY] Syscall spy library loaded (LD_PRELOAD)
[SPY] Intercepting: open, openat, read, write, close
[SPY] ═══════════════════════════════════════════════

[SPY] open("/proc/sys/crypto/fips_enabled", flags=O_RDONLY (0x0)) = -1 [errno=2: Нет такого файла или каталога]
[SPY] write(fd=4, buf=0x5ab01dba2270, count=416) = 416
[SPY] read(fd=4, buf=0x5ab01db99043, count=5) = -1 [errno=11: Ресурс временно недоступен]
[SPY] read(fd=4, buf=0x5ab01db99043, count=5) = 5
[SPY] read(fd=4, buf=0x5ab01db99048, count=122) = 122
...
[SPY] read(fd=4, buf=0x5ab01db99048, count=29) = 29

[SPY] ═══════════════════════════════════════════════
[SPY] Syscall spy library unloaded
[SPY] ═══════════════════════════════════════════════
```

**Анализ:**

* **Всего вызовов:** 300+ (в основном read)
* **Паттерн:** чередование мелких read(5 байт) и крупных read(1400–16401 байт)
* **Особенности:** EAGAIN (errno=11) при неблокирующем сокете; чтение порциями ~1400 байт (TCP MSS)

### Сравнение

| Программа | open/openat | read | write | close |
| --------- | ----------- | ---- | ----- | ----- |
| **curl**  | 4           | ~5   | 1     | 6     |
| **wget**  | 1           | 300+ | 2     | 0     |

**Выводы:**

* **wget** выполняет ~60 раз больше вызовов из-за неблокирующего I/O
* **curl** более оптимизирован — минимизирует обращения в ядро
* `/proc/sys/crypto/fips_enabled` открывается обеими программами при HTTPS
* **EAGAIN** нормален для неблокирующих сокетов

### Эксперимент со статической программой

**Создание:**

```bash
cat > hello_static.c << 'EOF'
#include <stdio.h>
int main() {
    FILE *f = fopen("/tmp/test.txt", "w");
    if (f) {
        fprintf(f, "Hello from static binary\n");
        fclose(f);
    }
    printf("Done\n");
    return 0;
}
EOF

gcc -static -o hello_static hello_static.c
```

**Проверка:**

```bash
ldd hello_static
	не является динамическим исполняемым файлом

file hello_static
hello_static: ELF 64-bit LSB executable, x86-64, statically linked, BuildID[sha1]=c5ba5586e58ed35d0140ca62c9ef21b31ac15ed6
```

**Запуск:**

```bash
LD_PRELOAD=./libsyscall_spy.so ./hello_static
Done
```

**Результат:** перехват не работает — библиотека не загружается.
**Причина:** статическая программа не использует динамический линкер, функции встроены при компиляции. Отсутствуют PLT/GOT и механизм подмены функций во время выполнения.

---

## Задание B: Benchmark системных вызовов

### Исходный код

Бенчмарк использует:

* **RDTSC** (`__rdtsc()`) для подсчёта циклов CPU
* **`clock_gettime(CLOCK_MONOTONIC)`** для времени в наносекундах
* **Статистику:** среднее, медиана, минимум, максимум, стандартное отклонение
* **Batch-замеры** для снижения шумов

### Результаты (1 млн итераций)

#### Холодный кэш

| Операция              | Среднее (ns) | Медиана | Min     | Циклы | Замедление |
| --------------------- | ------------ | ------- | ------- | ----- | ---------- |
| dummy()               | 6.03         | 2.40    | 2.40    | 6     | 1.0x       |
| getpid()              | 481.62       | 478.77  | 477.08  | 1447  | 79.9x      |
| open()+close()        | 2399.82      | 2401.51 | 2337.58 | 7187  | 398.0x     |
| gettimeofday() [vDSO] | 16.48        | 16.40   | 16.39   | 49    | 2.7x       |

#### Горячий кэш

| Операция              | Среднее (ns) | Медиана | Min     | Циклы | Замедление |
| --------------------- | ------------ | ------- | ------- | ----- | ---------- |
| dummy()               | 3.04         | 2.34    | 2.34    | 7     | 1.0x       |
| getpid()              | 477.06       | 477.05  | 464.69  | 1430  | 157.0x     |
| open()+close()        | 2245.33      | 2251.83 | 2191.00 | 6761  | 738.9x     |
| gettimeofday() [vDSO] | 16.97        | 16.39   | 15.99   | 48    | 5.6x       |

### Влияние кэша

| Операция       | Холодный | Горячий | Разница | Ускорение |
| -------------- | -------- | ------- | ------- | --------- |
| open()+close() | 2399.82  | 2245.33 | 154.49  | 1.07x     |
| getpid()       | 481.62   | 477.06  | 4.56    | 1.01x     |
| gettimeofday() | 16.48    | 16.97   | -0.49   | ~1.0x     |

**Вывод:** кэш ускоряет `open()` на 7% благодаря кешированию inode/dentry.

### Проверка через perf stat

```bash
sudo perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

**Результат:**

```
 Performance counter stats for './benchmark':
         3,459,162      cycles
         3,755,574      instructions              #    1.09  insn per cycle
                 0      context-switches
               127      page-faults
       0.001594825 seconds time elapsed
```

**Интерпретация:**

* **0 context switches** — выполнение без прерываний
* **127 page faults** — только при инициализации
* **IPC = 1.09** — эффективная загрузка конвейера
* **Sys time ≈ 1.6 мс** — основное время в ядре

---

## Ответы на вопросы

### 1. Что такое системный вызов и отличие от функции?

Системный вызов — способ программы обратиться к ядру ОС. Выполняется в Ring 0, требует переключения контекста и проверки прав. Обычная функция — в Ring 3, без перехода в ядро.

### 2. Почему системный вызов медленнее функции?

`getpid()` в ~80 раз медленнее `dummy()` из-за:

1. Переключения контекста и стека
2. Проверки прав и аргументов
3. Переходов Ring 3 ↔ Ring 0
4. Синхронизации структур ядра

### 3. Как работает LD_PRELOAD и когда не работает?

Загрузчик динамических библиотек подставляет функции из `LD_PRELOAD` перед системными.
Не работает:

* со статическими бинарниками
* с setuid/setgid
* при прямом вызове `syscall()`

### 4. Что такое vDSO?

vDSO — область памяти, где ядро размещает реализации часто вызываемых функций (`gettimeofday()`, `clock_gettime()`), что позволяет их вызывать без перехода в ядро и экономит время.

### 5. Почему open() медленнее getpid()?

`open()` требует:

1. Разрешение пути и проверку прав
2. Доступ к inode и dentry
3. Выделение дескриптора
4. Возможный I/O доступ к диску
   В среднем в 5 раз медленнее из-за сложности логики и обращения к ФС.

---

## Общие выводы

1. Системные вызовы значительно дороже функций — из-за переходов в режим ядра.
2. Буферизация критична — `curl` делает 15 вызовов против 300+ у `wget`.
3. vDSO ускоряет простые вызовы, устраняя переход в kernel mode.
4. Кэш улучшает производительность файловых операций (~7%).
5. LD_PRELOAD — удобный инструмент анализа, но не работает со статикой и setuid.

---

## Воспроизводимость

```bash
# Сборка
cd src/
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl
g++ -O2 -o benchmark benchmark.cpp

# Задание A
LD_PRELOAD=./libsyscall_spy.so curl -sS -o dota_json.txt "https://store.steampowered.com/api/appdetails?appids=570&l=en"
LD_PRELOAD=./libsyscall_spy.so wget -q -O steam_dota.html "https://store.steampowered.com/app/570/?l=english"

# Задание B
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches' && ./benchmark
./benchmark
sudo perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
```

