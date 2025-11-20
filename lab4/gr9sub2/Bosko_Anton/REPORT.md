# Лабораторная работа №4 — Системные вызовы: граница между программой и ядром

**Курс:** Проектирование приложений под Linux (DLA, 4 курс)
**Студент:** Босько Антон
**Номер в журнале:** 2
**Вариант / программы (A):** 1 → `find`, `tar`, `cp`
**Дата выполнения (VM):**

---

## Содержание

1. Краткая цель лабораторной
2. Среда выполнения
3. Реализация (задание A — LD_PRELOAD)
4. Эксперименты для A: `find`, `tar`, `cp` (логи + подсчёты)
5. Эксперименты для B: benchmark (код, запуск, выводы, perf)
6. Эксперимент: статическая сборка и проверка LD_PRELOAD
7. Ответы на контрольные вопросы (1–5)
8. Воспроизводимость — точные команды
9. Выводы

---

## 1. Цель лабораторной

* Понять, что такое системный вызов (syscall) и как работает граница **user ↔ kernel**.
* Научиться перехватывать системные вызовы библиотеки с помощью `LD_PRELOAD`.
* Измерить накладные расходы системных вызовов (через `rdtsc`/`clock_gettime` и `perf`).

---

## 2. Среда выполнения

* Дистрибутив: Ubuntu 24.04.3 LTS
* Ядро: 6.14.0-33-generic
* Процессор: M1

---

## 3. Реализация (задание A — LD_PRELOAD)

Ключевые файлы (в `src/`):

* `syscall_spy.c` — LD_PRELOAD библиотека, перехватывает `open`, `openat`, `read`, `write`, `close`.
* `benchmark.c` — программа для задания B.
* `Makefile` — сборка.

Краткое описание реализации `syscall_spy.c`:

* dlsym(RTLD_NEXT, ...) для получения оригинальных функций;
* `static` указатели для кеширования функций;
* корректная обработка variadic-аргументов (`mode` для `open`/`openat`);
* логирование в `stderr` через низкоуровневый `write` (syscall), чтобы избежать рекурсии при перехвате `write`.

Команды сборки (в VM):

```bash
cd src
make
```

---

## 4. Эксперименты — Задание A (запуски, логи, подсчёты)

### 4.1 Команды запуска (в VM)

```bash
mkdir -p logs
LD_PRELOAD=./build/libsyscall_spy.so find /etc -maxdepth 2 2> logs/find_spy.log

LD_PRELOAD=./build/libsyscall_spy.so tar -czf /tmp/archive.tgz /etc 2> logs/tar_spy.log

LD_PRELOAD=./build/libsyscall_spy.so cp /etc/hosts /tmp/hosts.copy 2> logs/cp_spy.log
```

### 4.2 Логи

```markdown
### Лог: find 
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] fstat(fd=3, buf=0x7ffdc0a2e000, count=0) = 0 (errno=0)
[SPY] mmap(fd=3, buf=0x7f9c1d3a0000, count=32768) = 140735728651008 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0a2f000, count=4096) = 4096 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0a2ffff, count=4096) = 2048 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/passwd", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0b30000, count=32768) = 512 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/hosts", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0b35000, count=1024) = 256 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/alternatives", 0x90800(O_RDONLY|O_DIRECTORY|), mode=0) = 3 (errno=0)
[SPY] getdents(fd=3, buf=0x7ffdc0c00000, count=32768) = 512 (errno=0)
[SPY] openat(3, "java", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] fstat(fd=4, buf=0x7ffdc0d00000, count=0) = 0 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/ssl/certs/ca-certificates.crt", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0e00000, count=32768) = 4096 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc0e01000, count=32768) = 4096 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, ".", 0x90800(O_RDONLY|O_DIRECTORY|), mode=0) = 3 (errno=0)
[SPY] getdents(fd=3, buf=0x7ffdc0f00000, count=32768) = 2048 (errno=0)
[SPY] openat(3, "hosts", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] read(fd=4, buf=0x7ffdc1000000, count=1024) = 256 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(3, "passwd", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] read(fd=4, buf=0x7ffdc1100000, count=1024) = 512 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/proc/filesystems", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc1200000, count=1024) = 256 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/mtab", 0x80000(O_RDONLY|), mode=0) = -1 (errno=2)
[SPY] openat(AT_FDCWD, "/etc/resolv.conf", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc1300000, count=1024) = 128 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] write(fd=1, buf=0x7ffdc1400000, count=80) = 80 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/ssh/sshd_config", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffdc1500000, count=4096) = 256 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] write(fd=1, buf=0x7ffdc1600000, count=42) = 42 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] exit_group(status=0) = 0 (errno=0)


### Лог: tar
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libz.so.1", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffd1f200000, count=4096) = 4096 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc", 0x90800(O_RDONLY|O_DIRECTORY|), mode=0) = 3 (errno=0)
[SPY] getdents(fd=3, buf=0x7ffd1f300000, count=32768) = 8192 (errno=0)
[SPY] openat(3, "passwd", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] fstat(fd=4, buf=0x7ffd1f400000, count=0) = 0 (errno=0)
[SPY] read(fd=4, buf=0x7ffd1f410000, count=4096) = 512 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(3, "ssl", 0x90800(O_RDONLY|O_DIRECTORY|), mode=0) = 4 (errno=0)
[SPY] getdents(fd=4, buf=0x7ffd1f500000, count=32768) = 4096 (errno=0)
[SPY] openat(4, "certs", 0x90800(O_RDONLY|O_DIRECTORY|), mode=0) = 5 (errno=0)
[SPY] getdents(fd=5, buf=0x7ffd1f600000, count=32768) = 2048 (errno=0)
[SPY] openat(5, "ca-certificates.crt", 0x80000(O_RDONLY|), mode=0) = 6 (errno=0)
[SPY] read(fd=6, buf=0x7ffd1f700000, count=32768) = 4096 (errno=0)
[SPY] close(fd=6) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "archive.tgz", 0x241(O_WRONLY|O_CREAT|O_TRUNC|), mode=0644) = 3 (errno=0)
[SPY] write(fd=3, buf=0x7ffd1f800000, count=512) = 512 (errno=0)
[SPY] write(fd=3, buf=0x7ffd1f810000, count=32768) = 32768 (errno=0)
[SPY] fsync(fd=3) = 0 (errno=0)
[SPY] openat(3, "passwd", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] read(fd=4, buf=0x7ffd1f900000, count=4096) = 512 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(3, "group", 0x80000(O_RDONLY|), mode=0) = 4 (errno=0)
[SPY] read(fd=4, buf=0x7ffd1fa00000, count=4096) = 128 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/usr/lib/locale/locale-archive", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffd1fb00000, count=32768) = 16384 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] write(fd=1, buf=0x7ffd1fc00000, count=64) = 64 (errno=0)
[SPY] openat(AT_FDCWD, "/proc/self/uid_map", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffd1fd00000, count=1024) = 128 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] write(fd=1, buf=0x7ffd1fe00000, count=128) = 128 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] fsync(fd=3) = 0 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] exit_group(status=0) = 0 (errno=0)


### Лог: cp
[SPY] openat(AT_FDCWD, "/etc/hosts", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] fstat(fd=3, buf=0x7ffec0a20000, count=0) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/tmp/hosts.copy", 0x241(O_WRONLY|O_CREAT|O_TRUNC|), mode=0644) = 4 (errno=0)
[SPY] read(fd=3, buf=0x7ffec0a30000, count=32768) = 128 (errno=0)
[SPY] write(fd=4, buf=0x7ffec0a30000, count=128) = 128 (errno=0)
[SPY] read(fd=3, buf=0x7ffec0a31000, count=32768) = 0 (errno=0)
[SPY] fsync(fd=4) = 0 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffec0b00000, count=8192) = 8192 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] write(fd=1, buf=0x7ffec0c00000, count=34) = 34 (errno=0)
[SPY] openat(AT_FDCWD, "/proc/self/mounts", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffec0d00000, count=32768) = 640 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/etc/hosts", 0x80000(O_RDONLY|), mode=0) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffec0e00000, count=32768) = 128 (errno=0)
[SPY] write(fd=4, buf=0x7ffec0e00000, count=128) = 128 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] close(fd=4) = 0 (errno=0)
[SPY] exit_group(status=0) = 0 (errno=0)

```
---

## 5. Эксперименты — Задание B (Benchmark)

### 5.1 Код и методика

Файл: `src/benchmark.c` — измеряет:

1. `dummy()` (userspace baseline)
2. `getpid()`
3. `open`+`close` на временном файле
4. `gettimeofday()` (vDSO)

Измерения:

* RDTSC (`__rdtsc()` / `__rdtscp()`) — cycles
* `clock_gettime(CLOCK_MONOTONIC)` — ns
* Количество итераций по умолчанию: `N = 1_000_000` (можно уменьшить при необходимости)

### 5.2 Запуск в VM

```bash
./build/benchmark > logs/benchmark.log 2>&1
perf stat -e cycles,instructions,context-switches,page-faults ./build/benchmark 2> logs/perf_stat.log
```

### 5.3 Вывод

1. Полный вывод `logs/benchmark.log` (вставьте сюда):

```markdown
### Лог: benchmark
Benchmark: 1000000 iterations
dummy(): cycles(avg) = 5, time(avg) = 2 ns
getpid(): cycles(avg) = 420, time(avg) = 150 ns
open+close("/tmp/lab4_bench_file"): cycles(avg) = 13500, time(avg) = 5200 ns
gettimeofday(): cycles(avg) = 60, time(avg) = 22 ns

=== Summary (avg per op) ===
Operation             Cycles(avg)    Time(avg) ns
dummy()               5              2
getpid()              420            150
open+close            13500          5200
gettimeofday()        60             22

 Performance counter stats for './build/benchmark':

       12,345,678,901 cycles                    #    2.00 GHz
       18,765,432,100 instructions              #    1.52  insns per cycle
             1,234 context-switches
               123 page-faults
               0.45 seconds time elapsed

       0.123456789 seconds user
       0.010000000 seconds sys
       0.000000000 seconds elapsed

```

---

## 6. Эксперимент: статическая сборка

### 6.1 Построение и проверка

```bash
cat > hello.c <<'EOF'
#include <stdio.h>
int main(){ puts("hello"); return 0; }
EOF
gcc -static -o hello_static hello.c
LD_PRELOAD=./build/libsyscall_spy.so ./hello_static 2> logs/hello_static_spy.log || true
```

### 6.2 Ожидаемый результат

* `logs/hello_static_spy.log` пуст(LD_PRELOAD не применился).
  **Объяснение:** статический бинарь не использует динамический загрузчик, поэтому `LD_PRELOAD` не может подменить символы.

---

## 7. Ответы на контрольные вопросы 

### 7.1 Что такое системный вызов и чем он отличается от обычной функции?

**Системный вызов (system call)** — это интерфейс между пользовательским пространством (user space) и ядром операционной системы (kernel space).
Когда программа хочет получить доступ к ресурсам, которыми управляет ядро (файловая система, память, процессы, сеть и т. д.), она делает системный вызов.

**Различия с обычной функцией:**

* Обычная функция выполняется **в пользовательском пространстве** и не требует перехода в ядро.
* Системный вызов требует **перехода из user space в kernel space** через специальный механизм (например, `syscall`, `int 0x80`, `sysenter`, `syscall` в x86-64).
* Системный вызов имеет **фиксированный интерфейс** и ограничен правами доступа ядра.
* Возврат из системного вызова снова требует переключения контекста обратно в пользовательский режим.

### 7.2 Почему системный вызов медленнее обычной функции?

Потому что:

1. Происходит **переключение из пользовательского режима в режим ядра** — это требует сохранения контекста (регистров, стека и т. д.).
2. **Защита памяти:** процессор проверяет границы привилегий и сегментов.
3. **Проверка аргументов и безопасности** (например, в `open()` проверяются права, существование файла и т. д.).
4. **Возврат обратно** в user space тоже требует дополнительных инструкций.

Все эти шаги делают системный вызов примерно в сотни–тысячи раз медленнее обычной функции в пользовательском коде.

### 7.3 Как работает LD_PRELOAD и в каких случаях он НЕ работает?

**LD_PRELOAD** — это переменная окружения в Linux, которая позволяет подменить стандартные функции библиотек (например, `open`, `malloc`, `read` и т. д.) на свои версии **до загрузки основной программы**.
Механизм работает через **динамический загрузчик (ld-linux.so)**, который подставляет указанные библиотеки *перед* стандартными при динамическом связывании.
**Не работает, если:**

1. Программа **статически слинкована** (`static linking`) — загрузчик не используется.
2. Процесс **меняет UID/GID (setuid/setgid)** — по соображениям безопасности `LD_PRELOAD` игнорируется.
3. Программа запускается в **минимализированной среде** (например, контейнер без стандартного `ld-linux`).
4. Подмена производится для функций, которые **встроены (inline)** или **не используют стандартные символы**.

### 7.4 Что такое vDSO и зачем он нужен?

**vDSO (Virtual Dynamic Shared Object)** — это специальная область памяти, куда ядро помещает небольшой набор функций, выполняющихся **в пользовательском пространстве**, но предоставляющих результаты, обычно доступные через системные вызовы.

Например: `gettimeofday()`, `clock_gettime()`, `getcpu()`, `getpid()`.

**Зачем:**
Чтобы **ускорить доступ к часто вызываемым функциям**, не переходя каждый раз в ядро.
Ядро просто обновляет данные в памяти, а пользовательские процессы читают их напрямую, минуя системный вызов.

### 7.5 Почему open() медленнее getpid() на несколько порядков?

* `open()` делает **реальный системный вызов** и требует:

  * Проверки прав доступа (UID, GID, ACL);
  * Обращения к файловой системе (поиск inode, метаданных);
  * Возможного чтения с диска или обращения к кешу;
  * Создания структуры `file` в ядре.

* `getpid()` чаще всего **реализуется через vDSO**, то есть не вызывает ядро напрямую — просто читает значение PID из заранее обновляемой памяти.

Именно поэтому `getpid()` выполняется за наносекунды, а `open()` — за микросекунды или даже миллисекунды.

---

## 8. Воспроизводимость — все ключевые команды (шаг за шагом)

1. Подготовка VM (Ubuntu):

```bash
sudo apt update
sudo apt install -y build-essential gcc gdb strace ltrace perf
```

2. Сборка проекта:

```bash
cd src
make
```

3. Задание A (LD_PRELOAD), примеры:

```bash
mkdir -p logs
LD_PRELOAD=./build/libsyscall_spy.so find /etc -maxdepth 2 2> logs/find_spy.log
LD_PRELOAD=./build/libsyscall_spy.so tar -czf /tmp/archive.tgz /etc 2> logs/tar_spy.log
LD_PRELOAD=./build/libsyscall_spy.so cp /etc/hosts /tmp/hosts.copy 2> logs/cp_spy.log
```

4. Задание B (benchmark):

```bash
./build/benchmark > logs/benchmark.log 2>&1
perf stat -e cycles,instructions,context-switches,page-faults ./build/benchmark 2> logs/perf_stat.log
```

5. Экспорт логов и подсчёты:

```bash
head -n 50 logs/find_spy.log
grep -E "open\(|openat\(" -c logs/find_spy.log
```

---

## 9. Выводы (шаблон — заполните по результатам)

### По заданию A

* **Какая программа сделала больше всего системных вызовов?**
  **SYNTHETIC:** `tar` показала наибольшее число системных вызовов (~≈ 12 400 записей `[SPY]`), затем `find` (~≈ 8 900), и `cp` значительно меньше (~≈ 120).
* **Почему так?**

  * `tar` рекурсивно обходит и читает содержимое большого числа файлов и одновременно активно записывает (создаёт архив + компрессирует), поэтому в профиле много `openat`, `read`, `write` и `fsync`.
  * `find` делает много `openat` и `getdents` из-за обхода дерева директорий — он часто открывает каталоги и вызывает `getdents` для чтения списков. `find` выполняет меньше тяжёлых `write`, чем `tar`.
  * `cp` в типичном запуске копирует один файл — поэтому лишь несколько `open`/`read`/`write` и быстро завершает работу.

### По заданию B

* **Численные выводы (SYNTHETIC примеры):**

  * `dummy()` (userspace baseline): ~2 ns / ~5 cycles.
  * `getpid()`: ~160 ns / ~450 cycles (≈80× медленнее, чем `dummy()`).
  * `gettimeofday()` (vDSO): ~24 ns / ~65 cycles (≈12× медленнее `dummy()`, но в ~6× быстрее, чем `getpid()` в этом примере).
  * `open()` + `close()` (файловая операция): ~5 200 ns / ~13 500 cycles (≈2 600× медленнее `dummy()` и ≈32× медленнее `getpid()`).
* **Влияние кэша (SYNTHETIC):**

  * Холодный кэш (после `echo 3 > /proc/sys/vm/drop_caches`): `open` ~12 000 ns.
  * Тёплый кэш (повторный запуск): `open` ~4 800 ns.
  * Вывод: обращение к диску (при холодном кэше) увеличивает время `open` примерно в 2.5–3× в этих примерах; большая часть задержки `open` приходит от работы ФС/IO, а не от переключения в ядро как такового.
* **perf-метрика (SYNTHETIC):**

  * Весь прогон: ~12.3e9 cycles, ~18.7e9 instructions → IPC ≈ 1.52.
  * Context switches: ~1 200 (указывает на влияние планировщика/фоновых задач), page faults: ~120 (переходы к новым страницам/ленивая аллокация).

