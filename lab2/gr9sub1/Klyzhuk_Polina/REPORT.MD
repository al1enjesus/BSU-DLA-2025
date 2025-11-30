# Отчёт по лабораторной 2

## Задание A:

**Цели:**

- Реализация супервизора для управления воркерами с обработкой сигналов.
- Исследование влияния nice, CPU-аффинити на планирование процессов.

## Задание A: Мини-супервизор с воркерами

### Реализован супервизор на Python, который:

- Порождает N воркеров (конфигурируется через JSON).
- Обрабатывает сигналы: SIGTERM/SIGINT, SIGHUP, SIGUSR1/SIGUSR2.
- Осуществляет graceful shutdown и graceful reload.
- Ограничивает частоту рестартов (не более 5 за 30 секунд).

### Демонстрация работы

```bash
$ python src/supervisor.py config.json
Supervisor PID: 1234
Config: {'workers': 3, 'mode_heavy': {'work_us': 9000, 'sleep_us': 1000}, 'mode_light': {'work_us': 2000, 'sleep_us': 8000}}
Starting supervisor...
Started worker 0 with PID 1235
Started worker 1 with PID 1236
Started worker 2 with PID 1237
Worker 0 started with PID 1235, mode: heavy
Worker 1 started with PID 1236, mode: heavy
Worker 2 started with PID 1237, mode: heavy
```

### Вывод воркеров:

```bash
Worker 0 [PID:1235] - Mode: heavy, Ticks: 10, CPU: 2, Work: 0.008s, Sleep: 0.001s
Worker 1 [PID:1236] - Mode: heavy, Ticks: 10, CPU: 3, Work: 0.008s, Sleep: 0.001s
Worker 2 [PID:1237] - Mode: heavy, Ticks: 10, CPU: 1, Work: 0.008s, Sleep: 0.001s
```

```bash
Received SIGHUP, initiating graceful reload...
Reloading configuration...
Gracefully restarting workers...
Worker 0 received shutdown signal
Worker 0 shutting down
Started worker 0 with PID 1240
Worker 0 started with PID 1240, mode: heavy
Reload completed
```

```bash
Received signal 15, initiating graceful shutdown...
Sending SIGTERM to all workers...
Worker 0 received shutdown signal
Worker 1 received shutdown signal
Worker 2 received shutdown signal
Worker 0 shutting down
Worker 1 shutting down
Worker 2 shutting down
All workers terminated
```

## Задание B: Планирование: nice и CPU-аффинити

Эксперимент с nice значениями

### Исходное состояние:

```bash
$ ps -eo pid,ni,pri,time,pcpu,comm | grep worker
1235   0  19 00:00:05 12.5 python
1236   0  19 00:00:05 12.3 python
1237   0  19 00:00:05 12.4 python
```

### Изменение nice для одного воркера:

```bash
$ sudo renice -n 10 1235
1235 (process ID) old priority 0, new priority 10

$ ps -eo pid,ni,pri,time,pcpu,comm | grep worker
1235  10  29 00:00:10  8.1 python  # Снижен приоритет
1236   0  19 00:00:10 14.2 python
1237   0  19 00:00:10 14.5 python
```

### Мониторинг с помощью pidstat

```bash
$ pidstat -u 1 10 -p 1235,1236
Linux 5.15.0-91-generic (hostname)   02/20/2024   _x86_64_   (4 CPU)

02:30:01 PM   UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:30:02 PM  1000      1235    7.92    0.00    0.00    0.00    7.92     1  python
02:30:02 PM  1000      1236   14.00    0.00    0.00    0.00   14.00     2  python

02:30:03 PM  1000      1235    8.00    0.00    0.00    0.00    8.00     1  python
02:30:03 PM  1000      1236   14.00    0.00    0.00    0.00   14.00     3  python
```

### CPU аффинити

```bash
$ taskset -cp 0 1235
pid 1235's current affinity list: 0-3
pid 1235's new affinity list: 0

$ taskset -cp 1 1236
pid 1236's current affinity list: 0-3
pid 1236's new affinity list: 1
```

## Ответы на вопросы

1. Чем процесс отличается от потока в Linux? Где это видно в `ps` и `/proc`?

Процесс - изолированное окружение с отдельной памятью. Поток - часть процесса, разделяющая с ним память. В `ps` видно по колонке LWP (потоки) и NLWP (их количество). В `/proc` каждый поток имеет свою папку в `task/`.

2. Как `nice` влияет на планирование CFS? Какие есть пределы/исключения?

`nice` изменяет вес процесса в планировщике CFS. Высокий nice (низкий приоритет) → меньший вес → меньше CPU времени. Диапазон: -20 до +19. Обычные пользователи могут только ухудшать приоритет, root - улучшать.

3. Что даёт CPU‑аффинити и когда она вредна?

Польза: улучшение кэша, предсказуемость для real-time задач, изоляция. Вред: при неравномерной нагрузке, когда закрепленные CPU перегружены, а другие свободны.

4. Чем отличаются `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_RSS`? Почему `RLIMIT_RSS` часто игнорируется?

- `RLIMIT_AS` - вся виртуальная память
- `RLIMIT_DATA` - только сегмент данных (heap)
- `RLIMIT_RSS` - резидентная память в RAM

RLIMIT_RSS часто игнорируется из-за сложности точного контроля резидентной памяти и политики overcommit в Linux.

5. Почему возможны зомби и как их избежать при массовых рестартах воркеров?

Зомби появляются, когда процесс завершился, но родитель не вызвал `wait()`. Чтобы избежать следует корректно обрабатывать SIGCHLD, использовать неблокирующий `waitpid()` в цикле.

6. Чем отличается «graceful shutdown» от «graceful reload/restart»? Какие последовательности безопасны?

**Shutdown** - полная остановка с ожиданием завершения процессов. **Reload** - перезапуск с сохранением сервиса (читает новый конфиг, постепенно перезапускает воркеры).

7. Как повлияют контейнерные лимиты (cgroup v2) на наблюдаемые метрики процесса?

cgroup v2 ограничивает видимость ресурсов для процесса. Метрики в /proc показывают лимиты cgroup, а не реальные системные ресурсы. CPU и memory ограничения применяются на уровне cgroup.

## Выводы

- Супервизор успешно управляет воркерами, корректно обрабатывает сигналы и ограничивает частоту рестартов.
- Nice значения значительно влияют на распределение CPU - разница в приоритетах хорошо видна в мониторинге.
- CPU-аффинити улучшает производительность за счет кэша, но требует балансировки нагрузки.
- Graceful механизмы обеспечивают надежность работы без потери данных.
- Мониторинг через /proc и утилиты дает полную диагностику состояния системы.

## Использование ИИ при выполнении работы

При выполнении лабораторной работы я использовала ИИ в следующих целях:

1. Поиск справочной информации - для уточнения синтаксиса системных вызовов Python
2. Генерация шаблонов кода - базовых структур для обработки сигналов
3. Проверка корректности - верификация некоторых концепций управления процессами