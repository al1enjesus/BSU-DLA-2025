# Отчёт по лабораторной работе 2
## Продвинутые процессы Linux: сигналы, планирование, ресурсы, /proc

### Цели работы
- Реализация системы супервизор-воркеры с управлением через сигналы
- Исследование влияния nice и CPU affinity на планирование
- Работа с ограничениями ресурсов и метриками /proc
- Освоение инструментов диагностики (pidstat, strace, perf)

### 1. Реализация супервизора и воркеров

#### Архитектура
Система состоит из:
- **Супервизор**: управляет жизненным циклом воркеров, обрабатывает сигналы
- **Воркеры**: выполняют полезную нагрузку, реагируют на сигналы управления

## A) Мини‑супервизор с воркерами

Цель:

Создать процесс, который будет создавать воркеров (дочерних процессов), следить за их состоянием и настраивает действия на определенные сигналы.

# Просмотр всех процессов

ps aux | grep -E "(supervisor|worker)"

Вывод:
qooriq           39398  89.2  0.0 408495760   1152 s000  R+   12:28PM   0:17.28 ./worker
qooriq           39397  87.8  0.0 408495760   1152 s000  R+   12:28PM   0:17.27 ./worker
qooriq           39399  87.3  0.0 408495760   1152 s000  R+   12:28PM   0:17.28 ./worker
qooriq           39396   0.0  0.0 408495760   1152 s000  S+   12:28PM   0:00.01 ./supervisor

### Переключить в легкий режим:
```bash 
kill -USR1 PID
```

````
Worker 42706: tick=1630, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42705: tick=1631, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42707: tick=1631, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42706: tick=1631, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42705: tick=1632, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42707: tick=1632, mode=LIGHT, work_us=2000, sleep_us=8000
Worker 42706: tick=1632, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42705: tick=1633, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42706: tick=1633, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42707: tick=1633, mode=LIGHT, work_us=2000, sleep_us=8000
````

### Переключить в тяжелый режим:
```bash
kill -USR2 PID
```

### Тестирование graceful reload:

``` bash
kill -HUP PID
```
````
Supervisor: Reloading configuration and restarting workers...
Worker 42683: Shutting down gracefully
Worker 42682: Shutting down gracefully
Worker 42684: Shutting down gracefully
Supervisor: Started worker with PID 42685
Supervisor: Started worker with PID 42686
Supervisor: Started worker with PID 42687
Worker started with PID: 42687
Worker started with PID: 42685
Worker 42685: tick=0, mode=HEAVY, work_us=9000, sleep_us=1000
Worker 42687: tick=0, mode=HEAVY, work_us=9000, sleep_us=1000
Worker started with PID: 42686
````

### Graceful shutdown:
``` bash
kill -TERM PID
```

````
Worker 42741: tick=2415, mode=HEAVY, work_us=9000, sleep_us=1000
Supervisor: Starting graceful shutdown...
Supervisor: Sending SIGTERM to worker 42741
Supervisor: Sending SIGTERM to worker 42742
Supervisor: Sending SIGTERM to worker 42743
Worker 42742: Shutting down gracefully
Worker 42743: Shutting down gracefully
Worker 42741: Shutting down gracefully
Supervisor: Worker 42743 terminated
Supervisor: Worker 42742 terminated
Supervisor: Worker 42741 terminated
Supervisor: Shutdown complete
````

# В) Планирование: nice и CPU‑аффинити

На macOS нет поддержки CPU-аффинити

Запуск 2-ух ворекров:
``` bash
./worker &
WORKER1_PID=$!
./worker &
WORKER2_PID=$!
```

***Установка разных nice значений:***
``` bash
sudo renice -n 0 WORKER1_PID
sudo renice -n 10 WORKER2_PID
```

***Мониторинг использования CPU***

``` bash
top -pid $WORKER2_PID -stats pid,command,cpu,time -o cpu -l 3
```
Выводы:
```
PID    COMMAND %CPU TIME    
45769  worker  87.4 01:50.29
PID    COMMAND %CPU TIME    
45768  worker  87.8 02:54.37
```

***Проверка текущих nice значений:***
``` bash
ps -o pid,ni,command -p $WORKER1_PID,$WORKER2_PID
```

Вывод:
```
PID NI COMMAND
45680 15 ./worker
45681  5 ./worker
```

***Остановка воркеров:***
``` bash
kill -TERM WORKER1_PID WORKER2_PID
```

Вывод:
```
Worker 45680: Shutting down gracefully
[1]  - done       ./worker
Worker 45681: Shutting down gracefully
[2]  + done       ./worker
```

#### Обработка сигналов
```c
// Супервизор обрабатывает:
SIGTERM/SIGINT - graceful shutdown
SIGHUP - graceful reload  
SIGUSR1/2 - переключение режимов воркеров
SIGCHLD - отслеживание состояния воркеров

// Воркеры обрабатывают:
SIGTERM - корректное завершение
SIGUSR1/2 - смена режима работы
```

1. Чем процесс отличается от потока в Linux?

## Процесс:

Изолированное адресное пространство
Отдельные PID, файловые дескрипторы
В /proc/pid/ отдельная информация

## Поток:

Разделяет адресное пространство процесса
Имеет собственный TID, но общий PID
В /proc/pid/task/tid/ информация о потоке
В ps -eLf видно:

Процессы с разными PID
Потоки с одинаковым PID, но разными LWP

## Как nice влияет на планирование CFS?

CFS (Completely Fair Scheduler) использует nice для изменения веса процесса в планировщике:

nice -20: вес 88761, ~80% CPU при конкуренции
nice 0: вес 1024, нормальный приоритет
nice +19: вес 15, ~1% CPU при конкуренции
Формула: weight = 1024 / 1.25^nice

## Что даёт CPU affinity и когда она вредна?

Преимущества:

Улучшение cache locality
Предсказуемость производительности
Изоляция критичных процессов

Недостатки:

Дисбаланс загрузки ядер
Неэффективность при неравномерной нагрузке
Сложность управления

## Чем отличаются RLIMIT_AS, RLIMIT_DATA, RLIMIT_RSS?

RLIMIT_AS: максимальный размер виртуальной памяти
RLIMIT_DATA: максимальный размер сегмента данных
RLIMIT_RSS: максимальный размер резидентной памяти (часто игнорируется)
RLIMIT_RSS игнорируется в современных ядрах из-за сложности точного контроля RSS.

## Почему возможны зомби и как их избежать?

Зомби возникают когда:

Процесс завершился
Родитель не вызвал wait()
PID заблокирован до вызова wait()
Предотвращение:

Обработка SIGCHLD
Использование waitpid() с WNOHANG
Rate limiting рестартов

## Чем отличается graceful shutdown от graceful reload?

Graceful shutdown:

Послать SIGTERM воркерам
Дождаться их завершения (timeout 5s)
Завершить супервизор
Graceful reload:

Перечитать конфигурацию
Последовательно перезапустить воркеров
Сохранить общую доступность

## Как повлияют контейнерные лимиты (cgroup v2)?

cgroup v2 обеспечивает строгие ограничения:

memory.max: жесткий лимит памяти (OOM kill при превышении)
cpu.max: квота CPU времени
pids.max: ограничение числа процессов
Метрики в /proc отражают ограничения cgroup.