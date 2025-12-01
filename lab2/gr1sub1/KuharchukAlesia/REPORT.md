# Лабораторная работа 2
## Продвинутые процессы Linux: сигналы, планирование, ресурсы, /proc
**ОС для тестирования:** Ubuntu 22.04 LTS

## Цели работы

1. Реализация супервизора для управления worker-процессами с обработкой сигналов
2. Исследование влияния параметров планирования (nice, CPU affinity) на распределение ресурсов
3. Анализ метрик процессов через системные утилиты
4. Реализация graceful shutdown и graceful reload механизмов


## Обзор реализации

### Архитектура системы
- **Супервизор** - главный процесс, управляющий жизненным циклом воркеров
- **Воркеры** (3 процесса) - выполняют полезную нагрузку, обрабатывают сигналы
- **Конфигурация** - параметры работы системы в отдельном файле

### Обработка сигналов
- `SIGUSR1` - переход в LIGHT режим (меньше работы, больше сна)
- `SIGUSR2` - переход в HEAVY режим (больше работы, меньше сна)  
- `SIGHUP` - graceful reload конфигурации
- `SIGTERM/SIGINT` - graceful shutdown системы
- `SIGCHLD` - обработка завершения дочерних процессов

---

## Эксперименты и результаты

### 1. Запуск системы и инициализация

**Вывод при запуске:**
```bash
 === Supervisor started (PID: 7201) ===
Worker[0] 7203 started (parent: 7201)
Worker[1] 7204 started (parent: 7201)
Worker[2] 7205 started (parent: 7201)
Supervisor: Started 3 workers
Workers with even IDs: nice=0, CPU=0
Workers with odd IDs: nice=10, CPU=1
```


**Настройки планирования:**
- Worker 7203 (ID=0): nice=0, affinity=CPU0
- Worker 7204 (ID=1): nice=10, affinity=CPU1  
- Worker 7205 (ID=2): nice=0, affinity=CPU0

### 2. Анализ влияния nice на распределение CPU

**Данные из ps:**
```bash
alesia1 7203 30.0 0.0 2496 572 pts/1 R+ 13:26 0:00 worker 0 (nice=0)
alesia1 7204 3.0 0.0 2496 576 pts/1 RN+ 13:26 0:00 worker 1 (nice=10)
alesia1 7205 51.0 0.0 2496 580 pts/1 R+ 13:27 0:00 worker 2 (nice=0)
```

**Метрики pidstat (усредненные):**
```bash
Average: UID PID %usr %system %guest %wait %CPU CPU Command
Average: 1000 7203 3.79 17.15 0.00 42.37 20.94 - worker
Average: 1000 7204 1.69 7.18 0.00 83.85 8.87 - worker
Average: 1000 7205 5.18 24.03 0.00 54.34 29.21 - worker
```


**Выводы:**
- Процессы с nice=0 получают 20-30% CPU времени
- Процесс с nice=10 получает только 8.87% CPU времени  
- **Разница в распределении CPU:** в 2.5-3.5 раза
- Процессы с низким приоритетом (nice=10) проводят больше времени в состоянии ожидания (83.85% vs 42-54%)

### 3. Переключение режимов работы

**LIGHT режим (SIGUSR1):**
```bash
Supervisor: Switching workers to LIGHT mode
Worker 7203: Switching to LIGHT mode
Worker 7205: Switching to LIGHT mode
Worker 7204: Switching to LIGHT mode
Worker[1] PID=7204: mode=LIGHT, nice=10, ticks=501567
```


**HEAVY режим (SIGUSR2):**
```bash
Supervisor: Switching workers to HEAVY mode
Worker 7203: Switching to HEAVY mode
Worker 7205: Switching to HEAVY mode
Worker[0] PID=7203: mode=HEAVY, nice=0, ticks=1802017
Worker[2] PID=7205: mode=HEAVY, nice=0, ticks=1802989
Worker 7204: Switching to HEAVY mode
```


**Выводы:**
- Сигналы корректно доставляются всем воркерам
- Воркеры мгновенно реагируют на изменение режима работы
- Состояние синхронизировано между всеми процессами

### 4. Автоматический рестарт воркеров

**Тестирование отказоустойчивости:**
```bash
Simulating crash of worker 7203
Supervisor: Worker[0] 7203 killed by signal 9
Supervisor: Worker 7203 crashed, restarting...
Supervisor: Stopping worker 7203
Worker[0] 7259 started (parent: 7201)
Worker[0] PID=7259: mode=HEAVY, nice=0, ticks=500761
```


**Выводы:**
- Супервизор обнаруживает падение воркера через SIGCHLD
- Автоматический рестарт выполняется за 1-2 секунды
- Новый процесс наследует все параметры планирования
- Состояние (режим работы) сохраняется

### 5. Graceful reload конфигурации

**Выполнение reload:**
```bash
Supervisor: Received reload signal
Supervisor: Reloading configuration...
Reloading configuration...
```


**Выводы:**
- Система поддерживает "на лету" обновление конфигурации
- Процессы продолжают работу во время reload
- Минимальное воздействие на производительность

### 6. Graceful shutdown

**Процесс завершения:**
```bash
Supervisor: Received shutdown signal
Supervisor: Initiating graceful shutdown...
Supervisor: Stopping all workers...
Supervisor: Stopping worker 7259
Worker 7259: Received SIGTERM, shutting down...
Worker[0] 7259: Clean shutdown completed
Worker 7204: Received SIGTERM, shutting down...
Worker[1] 7204: Clean shutdown completed
Worker 7205: Received SIGTERM, shutting down...
Worker[2] 7205: Clean shutdown completed
```

**Выводы:**
- Все воркеры получают сигнал завершения
- Процессы корректно освобождают ресурсы
- Система завершает работу без остаточных процессов

### 7. Анализ использования памяти

**Системная память до/после:**
```bash
=== Memory info before start ===
MemTotal: 2011232 kB
MemFree: 143256 kB
MemAvailable: 538920 kB

=== Memory info after demo ===
MemTotal: 2011232 kB
MemFree: 239936 kB
MemAvailable: 639500 kB
```

**Использование памяти процессами:**
```bash
Average: UID PID VSZ RSS %MEM Command
Average: 1000 7201 2500 700 0.03 supervisor
Average: 1000 7203 2496 572 0.03 worker
Average: 1000 7204 2496 576 0.03 worker
Average: 1000 7205 2496 580 0.03 worker
```

**Выводы:**
- Каждый процесс использует ~2.5MB виртуальной памяти
- Резидентная память ~570KB на процесс
- Утечек памяти не обнаружено
- После завершения демо память освобождается

---

## Ответы на вопросы

### 1. Чем процесс отличается от потока в Linux? Где это видно в `ps` и `/proc`?

**Различия:**
- Процессы имеют независимые address space, потоки разделяют address space
- Создание процесса через fork(), потока через pthread_create()
- Процессы изолированы, потоки разделяют ресурсы

**В `ps`:**
- Процессы: разные PID, отдельные строки (`ps aux`)
- Потоки: показываются с флагом `-L` (`ps -eLf`), имеют одинаковый PID но разные LWP

**В `/proc`:**
- Каждый процесс: `/proc/PID/`
- Потоки процесса: `/proc/PID/task/TID/`
- Разделяемая память видна в maps обоих потоков

### 2. Как `nice` влияет на планирование CFS? Какие есть пределы/исключения?

**Влияние на CFS:**
- CFS (Completely Fair Scheduler) использует nice для расчета веса процесса
- Вес определяет долю CPU времени: weight = 1024 / (1.25 ^ nice)
- Процесс с nice=0 имеет вес 1024, с nice=10 - вес ~100
- В нашем эксперименте: nice=0 получал 20-30% CPU, nice=10 - ~9% CPU

**Пределы:**
- Обычные пользователи: nice от 0 до 19 (только повышение)
- root: может устанавливать nice от -20 до 19
- RT (real-time) процессы имеют приоритет над SCHED_NORMAL

### 3. Что даёт CPU-аффинити и когда она вредна?

**Преимущества:**
- Уменьшает переключения между ядрами (cache misses)
- Улучшает производительность для CPU-bound задач
- Позволяет изолировать критические процессы

**Когда вредна:**
- При imbalance нагрузки между ядрами
- Когда нужно использовать все доступные CPU ресурсы
- Для I/O-bound задач, где CPU не является bottleneck

**В нашем случае:** На системе с 1 CPU core affinity имеет ограниченный эффект.

### 4. Чем отличаются `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_RSS`? Почему `RLIMIT_RSS` часто игнорируется?

**RLIMIT_AS** - ограничение на размер виртуальной памяти (address space)
**RLIMIT_DATA** - ограничение на сегмент данных (heap)
**RLIMIT_RSS** - ограничение на резидентный размер (physical memory)

**RLIMIT_RSS игнорируется потому что:**
- Современные системы используют сложные схемы управления памятью
- RSS сложно контролировать из-за shared libraries, page cache
- Более эффективно использовать RLIMIT_AS или memory cgroups

### 5. Почему возможны зомби и как их избежать при массовых рестартах воркеров?

**Причина зомби:** Родительский процесс не вызывает wait()/waitpid() для завершенного дочернего процесса.

**В нашей реализации избегаем зомби через:**
- Обработчик SIGCHLD в супервизоре
- Вызов waitpid() с флагом WNOHANG в цикле
- Регулярная проверка состояния воркеров

```c
void check_workers() {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // обработка завершенного воркера
    }
}
```
---
## Использование ИИ-инструментов

При выполнении лабораторной работы использовались ИИ-инструменты для помощи в написании кода. Они помогли сгенерировать базовую структуру супервизора и воркеров, а также создать демонстрационные скрипты.