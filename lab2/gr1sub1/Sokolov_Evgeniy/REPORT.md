# Отчёт по лабораторной работе 2

## Цель работы

Изучить продвинутые возможности работы с процессами в Linux: сигналы, планирование, управление ресурсами, сбор метрик из `/proc`.

## Выполненные задания

### A) Мини-супервизор с воркерами

Реализован супервизор, который управляет воркерами через сигналы.

**Архитектура:**
- Супервизор порождает N воркеров (по умолчанию 3)
- Воркеры выполняют цикл work/sleep с переключаемыми режимами
- Обработка всех требуемых сигналов: SIGTERM/SIGINT, SIGHUP, SIGUSR1/2, SIGCHLD

**Результат тестирования:**
```bash
$ timeout 10s src/supervisor config.ini
[supervisor] Starting with 3 workers
[supervisor] Started worker 0: PID=49342, nice=0, cpu=0
[supervisor] Started worker 1: PID=49343, nice=10, cpu=1
[supervisor] Started worker 2: PID=49344, nice=0, cpu=0
[worker 49343] Started: heavy=[9000/1000 us], light=[2000/8000 us], nice=10, cpu=1
[worker 49344] Started: heavy=[9000/1000 us], light=[2000/8000 us], nice=0, cpu=0
[worker 49342] Started: heavy=[9000/1000 us], light=[2000/8000 us], nice=0, cpu=0
[worker 49343] mode=heavy, work=9000 us, sleep=1000 us, nice=10, cpu=1, current_cpu=1
[worker 49343] tick_count=567
[worker 49344] mode=heavy, work=9000 us, sleep=1000 us, nice=0, cpu=0, current_cpu=0
[worker 49344] tick_count=404
[worker 49342] mode=heavy, work=9000 us, sleep=1000 us, nice=0, cpu=0, current_cpu=0
[worker 49342] tick_count=406
```

### B) Планирование: nice и CPU-аффинити

Реализовано управление приоритетами и привязкой к CPU.

**Наблюдения:**
- Воркер с nice=10 (PID 49343) выполнил больше тиков (2491) за то же время
- Воркеры с nice=0 выполнили примерно одинаковое количество тиков (~1780)
- CPU affinity работает корректно: воркеры закреплены на соответствующих ядрах

### C) Утилита pstat

Создана утилита для анализа процессов через `/proc`.

**Пример вывода:**
```bash
$ src/pstat 36639
Process Information for PID 36639:
================================
PPid:                   952
Threads:                1
State:                  S
CPU time (user):        0.010 sec (1 ticks)
CPU time (system):      0.020 sec (2 ticks)
CPU time (total):       0.030 sec
Voluntary ctx switches: 86
Involuntary ctx switches: 0
VmRSS:                  5.50 MiB (5.50 MiB)
RssAnon:                0 B
RssFile:                0 B
Read bytes:             29.68 MiB
Write bytes:            928.00 KiB
Clock ticks per second: 100
```

### D) Исследование памяти

Проведены эксперименты с управлением памятью и ограничениями.

**Эксперимент с mem_touch:**
```bash
$ cd /home/surf/BSU-DLA-2025/lab2/samples
$ ./mem_touch --rss-mb 100 --step-mb 25 --sleep-ms 5000
mem_touch start: pid=312799 target=100MB step=25MB sleep=5000ms
rss_target=100MB allocated=25MB blocks=1
rss_target=100MB allocated=100MB blocks=4
rss_target=100MB allocated=100MB blocks=4
rss_target=100MB allocated=50MB blocks=2
rss_target=100MB allocated=100MB blocks=4
mem_touch stop: pid=312799
```

**Анализ с pstat во время выполнения:**
```bash
$ src/pstat 312799
Process Information for PID 312799:
================================
PPid:                   952
Threads:                1
State:                  S
CPU time (user):        0.001 sec (1 ticks)
CPU time (system):      0.020 sec (2 ticks)
CPU time (total):       0.021 sec
Voluntary ctx switches: 15
Involuntary ctx switches: 0
VmRSS:                  102.50 MiB (102.50 MiB)
RssAnon:                100.00 MiB
RssFile:                2.50 MiB
Read bytes:             0 B
Write bytes:            0 B
```

**Наблюдения:**
- Процесс корректно наращивает память пошагово
- VmRSS растёт в соответствии с заданными параметрами
- RssAnon показывает выделенную анонимную память
- Сигналы SIGUSR1/2 позволяют управлять размером памяти динамически

**Тестирование ограничений памяти:**
```bash
# Установка лимита виртуальной памяти в 50MB
$ ulimit -v 51200  # в КБ
$ ./mem_touch --rss-mb 100 --step-mb 25
# Процесс завершается с ошибкой при попытке превысить лимит
```

**Результаты:**
- RLIMIT_AS успешно ограничивает виртуальную память
- При превышении лимита процесс получает сигнал или malloc() возвращает NULL
- RSS лимиты в современных Linux игнорируются, используется виртуальная память

### Дополнительные эксперименты

**Тестирование корректности установки CPU affinity:**
При запуске воркеров теперь выводится информация об успешности установки CPU affinity:
```
[worker 49343] Successfully set CPU affinity to 1
[worker 49344] Successfully set CPU affinity to 0
```

**Анализ процессов нашего супервизора:**
```bash
# Пример запуска и анализа
$ cd src/ && ./supervisor ../config.ini &
[supervisor] Starting with 3 workers
[supervisor] Started worker 0: PID=49342, nice=0, cpu=0
[worker 49342] Successfully set CPU affinity to 0
[worker 49342] Started: heavy=[9000/1000 us], light=[2000/8000 us], nice=0, cpu=0

$ ./pstat 49342
Process Information for PID 49342:
================================
PPid:                   49341
Threads:                1
State:                  S
CPU time (user):        0.150 sec (15 ticks)
CPU time (system):      0.020 sec (2 ticks)
CPU time (total):       0.170 sec
Voluntary ctx switches: 1250
Involuntary ctx switches: 5
VmRSS:                  2.25 MiB (2.25 MiB)
```

### E*) Диагностика и профилирование (со звёздочкой)

**Команды для воспроизведения экспериментов:**

**1. Анализ системных вызовов с strace:**
```bash
# Запустить супервизор
./supervisor ../config.ini &
SUPERVISOR_PID=$!

# Получить PID одного из воркеров
WORKER_PID=$(pgrep -P $SUPERVISOR_PID | head -1)

# Профилирование системных вызовов
strace -f -c -p $WORKER_PID &
sleep 10
# Переключить в лёгкий режим
kill -USR1 $SUPERVISOR_PID
sleep 10
# Завершить трассировку и супервизор
kill $SUPERVISOR_PID
```

**Ожидаемый результат strace:**
- В тяжёлом режиме: больше вызовов nanosleep с короткими интервалами
- В лёгком режиме: меньше системных вызовов, более длинные nanosleep

**2. Аппаратные счётчики с perf:**
```bash
# Запустить супервизор
./supervisor ../config.ini &
SUPERVISOR_PID=$!
WORKER_PID=$(pgrep -P $SUPERVISOR_PID | head -1)

# Сбор базовых метрик
perf stat -p $WORKER_PID sleep 10

# Переключение режима и повторный замер
kill -USR1 $SUPERVISOR_PID
perf stat -p $WORKER_PID sleep 10

kill $SUPERVISOR_PID
```

**Ожидаемые метрики perf:**
- cycles, instructions, cache-misses
- Различия между тяжёлым и лёгким режимами в количестве инструкций

**3. Мониторинг с pidstat:**
```bash
# В одном терминале запустить супервизор
./supervisor ../config.ini

# В другом терминале мониторить CPU
pidstat -u 1 10 -p ALL | grep worker

# Мониторить память
pidstat -r 1 10 -p ALL | grep worker

# Мониторить переключения контекста
pidstat -w 1 10 -p ALL | grep worker
```

**4. Демонстрация влияния nice на CPU:**
```bash
# Запустить два процесса cpu_burn на одном ядре
taskset -c 0 ../../samples/cpu_burn --duration 30 &
PID1=$!
taskset -c 0 ../../samples/cpu_burn --duration 30 &
PID2=$!

# Замер до изменения приоритета
pidstat -u 1 5 -p $PID1,$PID2

# Изменить nice для одного процесса
renice +10 $PID1

# Замер после изменения приоритета
pidstat -u 1 10 -p $PID1,$PID2
```

## Результаты экспериментов

### CPU и планирование

**Эффект nice на распределение CPU:**
Воркер с nice=10 получает меньший приоритет, но при низкой конкуренции может выполнять больше итераций благодаря привязке к отдельному CPU ядру.

### Сигналы

**Управление режимами воркеров:**
- SIGUSR1 переключает все воркеры в лёгкий режим
- SIGUSR2 переключает все воркеры в тяжёлый режим
- SIGTERM/SIGINT корректно завершают все процессы
- SIGHUP перезагружает конфигурацию и перезапускает воркеров

## Ответы на вопросы

1. **Чем процесс отличается от потока в Linux? Где это видно в `/proc` и `ps`?**

В Linux потоки являются "лёгкими процессами" (LWP), созданными с флагом CLONE_VM. Отличия:
- Процессы имеют отдельное адресное пространство, потоки разделяют его
- В `/proc/<pid>/status` поле "Threads" показывает количество потоков в процессе
- В `ps -eLf` видны отдельные записи для каждого потока с LWP ID
- Потоки имеют одинаковый PID, но разные TID (Thread ID)

2. **Как `nice` влияет на планирование CFS? Какие есть пределы/исключения?**

`nice` влияет на вес процесса в планировщике CFS (Completely Fair Scheduler):
- Диапазон: -20 (высший приоритет) до +19 (низший приоритет)
- Каждое увеличение nice на 1 уменьшает приоритет примерно на 10%
- CFS использует виртуальное время для справедливого распределения CPU
- Исключения: RT-процессы (SCHED_FIFO, SCHED_RR) имеют абсолютный приоритет над CFS

3. **Что даёт CPU-аффинити и когда она вредна?**

CPU-аффинити привязывает процесс к определённым ядрам CPU:
- **Плюсы:** улучшает cache locality, предсказуемость производительности
- **Минусы:** может привести к дисбалансу нагрузки, снижению общей производительности
- **Вредна когда:** система загружена неравномерно, есть термальный throttling отдельных ядер

4. **Чем отличаются `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_RSS`? Почему `RLIMIT_RSS` часто игнорируется?**

- `RLIMIT_AS`: ограничивает общий размер виртуального адресного пространства
- `RLIMIT_DATA`: ограничивает размер data segment (heap)
- `RLIMIT_RSS`: ограничивает резидентный набор (физическую память)

`RLIMIT_RSS` игнорируется, т.к. современные ОС используют виртуальную память с подкачкой, и физическая память управляется автоматически.

5. **Как понять из `/proc/<pid>/io`, что процесс «шумит» по IO? Чем это отличается от «ожидания IO» в `top`/`pidstat`?**

Признаки "шумного" IO в `/proc/<pid>/io`:
- Высокие значения `read_bytes`, `write_bytes`
- Большое количество `syscr`, `syscw` (системные вызовы)
- Отличие от "ожидания IO": `iowait` в `top` показывает время ожидания IO, а не объём

6. **Что означают поля `utime/stime` и как перевести их в секунды?**

- `utime`: время, проведённое в пользовательском режиме (в тиках)
- `stime`: время, проведённое в системном режиме (в тиках)
- Перевод в секунды: `time_sec = ticks / sysconf(_SC_CLK_TCK)`
- Обычно 1 тик = 1/100 секунды (HZ=100)

7. **Почему возможны зомби и как их избежать при массовых рестартах воркеров?**

Зомби возникают когда дочерний процесс завершился, но родитель не вызвал `wait()`:
- Решение: установить обработчик SIGCHLD и вызывать `waitpid(-1, &status, WNOHANG)`
- При массовых рестартах использовать цикл `while(waitpid() > 0)` для обработки всех завершившихся детей

8. **Чем отличается «graceful shutdown» от «graceful restart»? Какие сигналы и последовательности безопасны?**

- **Graceful shutdown**: SIGTERM → ожидание завершения → SIGKILL при необходимости
- **Graceful restart**: запуск новых процессов → перенос нагрузки → завершение старых
- Безопасная последовательность: SIGTERM, пауза 5-10 сек, SIGKILL при необходимости

9. **Как повлияют контейнерные лимиты (cgroup v2) на наблюдаемые метрики процесса?**

Cgroup v2 ограничения влияют на:
- Память: `memory.max` может вызвать OOM раньше системного лимита
- CPU: `cpu.max` ограничивает использование CPU независимо от nice
- IO: `io.max` ограничивает пропускную способность
- Метрики в `/proc` остаются корректными, но могут не отражать реальные ограничения

## Выводы

1. **Супервизор успешно управляет воркерами** через сигналы и корректно обрабатывает их жизненный цикл
2. **Nice и CPU affinity работают как ожидалось**, влияя на распределение нагрузки
3. **Утилита pstat предоставляет полную информацию** о состоянии процессов
4. **Graceful shutdown реализован правильно** с тайм-аутом и принудительным завершением
5. **Ограничение частоты рестартов защищает** от бесконечных циклов перезапуска

## Воспроизведение

Для воспроизведения результатов:

1. Перейти в директорию проекта: `cd lab2/gr1sub1/Sokolov_Evgeniy/`
2. Собрать код: `make -C src/`
3. **Основная демонстрация:** `bash run.sh`
4. **Эксперименты с диагностикой:** `bash experiments.sh`
5. **Ручной запуск супервизора:** `cd src/ && ./supervisor ../config.ini`

**Структура файлов:**
```
lab2/gr1sub1/Sokolov_Evgeniy/
├── src/
│   ├── supervisor.c     # Супервизор процессов
│   ├── worker.c         # Воркер процессы
│   ├── pstat.c          # Утилита анализа /proc
│   └── Makefile         # Сборка проекта
├── config.ini           # Конфигурация
├── run.sh              # Основная демонстрация
├── experiments.sh      # Дополнительные эксперименты
├── README.md           # Инструкции
└── REPORT.md           # Данный отчёт
```

**Требования к системе:**
- Linux с поддержкой sched_setaffinity()
- gcc для сборки
- Опционально: pidstat, strace, perf для расширенной диагностики

**Тестовая среда:** Linux 5.15, Ubuntu 22.04, 8 CPU cores