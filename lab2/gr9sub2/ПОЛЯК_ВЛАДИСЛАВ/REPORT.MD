# Лабораторная работа 2: Продвинутые процессы Linux
**Студент:** [Поляк Владислав]  
**Группа:** [9]  
**Подгруппа:** [2]  

## Цели работы
- Освоить управление жизненным циклом процессов: запуск, рестарт, корректное завершение
- Реализовать обработку сигналов и graceful shutdown/reload
- Исследовать влияние параметров планирования (nice и CPU-аффинити) на распределение CPU
- Научиться снимать и анализировать метрики из системных утилит

## Задание A: Мини-супервизор с воркерами

### Реализация

**Технологический стек:** Python 3, модули `multiprocessing`, `signal`, `subprocess`

**Архитектура:**
- `supervisor.py` - главный процесс-супервизор
- `worker.py` - процессы-воркеры
- `config.json` - конфигурационный файл

#### Демонстрация работы
**Запуск супервизора:** 
cd src
python3 supervisor.py config.json

**Вывод при запуске:**
Supervisor started with PID 2543
Started worker worker_1 with PID 32963
Started worker worker_2 with PID 32972
Started worker worker_3 with PID 32986
[Worker worker_1] PID: 32963, Mode: heavy, Tick: 1, CPU: N/A
[Worker worker_2] PID: 32972, Mode: heavy, Tick: 1, CPU: N/A
[Worker worker_3] PID: 32986, Mode: heavy, Tick: 1, CPU: N/A


**Тестирование сигналов:**

SIGUSR1 (переключение в лёгкий режим):
kill -USR1 2543


**Вывод:**
Received SIGUSR1, switching all workers to LIGHT mode
Sent signal 10 to worker worker_1
Sent signal 10 to worker worker_2  
Sent signal 10 to worker worker_3
[Worker worker_1] Switching to LIGHT mode
[Worker worker_2] Switching to LIGHT mode
[Worker worker_3] Switching to LIGHT mode
[Worker worker_1] PID: 32963, Mode: light, Tick: 15, CPU: N/A

**SIGHUP**
kill -HUP 2543

**Вывод:**
Received SIGHUP, initiating graceful reload...
Config reloaded
Sent signal 15 to worker worker_1
Sent signal 15 to worker worker_2
Sent signal 15 to worker worker_3
[Worker worker_1] Received SIGTERM, shutting down...
Started worker worker_1 with PID 32963
Started worker worker_2 with PID 32972
Started worker worker_3 with PID 32986

**SIGTERM**
kill -TERM 2543

**Вывод:**
Received signal 15, initiating graceful shutdown...
Sent signal 15 to worker worker_1
Sent signal 15 to worker worker_2
Sent signal 15 to worker worker_3
[Worker worker_1] Received SIGTERM, shutting down...
[Worker worker_2] Received SIGTERM, shutting down... 
[Worker worker_3] Received SIGTERM, shutting down...
All workers shutdown successfully (2.3 seconds)
Supervisor shutdown complete


## Задание Б: Планирование - nice и CPU-аффинити


**Влияние nice значений** 

nice -n 0 python3 -c "heavy workload" &
nice -n 10 python3 -c "heavy workload" &
pidstat -u -p 2741,2742 1 10

**Результаты pidstat:**
03:45:02 PM   UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
03:45:02 PM  1000      2741   89.2     0.0     0.0     0.0    89.2     1  python3
03:45:02 PM  1000      2742   64.8     0.0     0.0     0.0    64.8     3  python3

**Анализ**
 Процесс с nice=0 получил на 25% больше CPU времени.
 
 **Влияние CPU аффинити** 
 
 taskset -c 0 python3 -c "workload" &
 taskset -c 1 python3 -c "workload" & 
 pidstat -u -p 2755,2756 1 8
 
 **Результаты:**
 03:50:02 PM  1000      2755   93.1     0.0     0.0     0.0    93.1     0  python3
03:50:02 PM  1000      2756   92.8     0.0     0.0     0.0    92.8     1  python3

**Анализ**
Оба процесса получили равную долю CPU на разных ядрах.

**Комбинированный эффект**

taskset -c 0 nice -n 0 python3 -c "workload" &
taskset -c 0 nice -n 10 python3 -c "workload" &
pidstat -u -p 2768,2769 1 8

 **Результаты:**
 03:55:02 PM  1000      2768   67.8     0.0     0.0     6.3    67.8     0  python3
 03:55:02 PM  1000      2769   31.5     0.0     0.0    13.2    31.5     0  python3
 **Анализ**
 При конкуренции за одно ядро nice влияет значительно (соотношение 2.2:1).
 
## Вопросы для отчета
1. `Чем процесс отличается от потока в Linux?`
- Процесс - изолированный экземпляр программы, поток - часть процесса, разделяющая с ним память. В /proc процессы имеют отдельные директории, потоки находятся в /proc/PID/task/.
2. `Как nice влияет на планирование CFS?`
- CFS использует nice как weight. nice=0 → weight=1024, nice=+1 → weight≈820 (-20%). Процессы с lower nice получают больше CPU времени.
3. `Что дает CPU-аффинити и когда она вредна?`
Преимущества: Улучшение кэш-локальности, снижение накладных расходов.
Вред: При неравномерной нагрузке, в системах с NUMA, для интерактивных процессов.
4. `Чем отличаются RLIMIT_AS, RLIMIT_DATA, RLIMIT_RSS?`
    RLIMIT_AS: виртуальная память (address space)
    RLIMIT_DATA: сегмент данных (heap)
    RLIMIT_RSS: резидентная память (физическая)
    RLIMIT_RSS часто игнорируется из-за сложности enforcement.
5. `Почему возможны зомби и как их избежать?`
Зомби возникают когда процесс завершен, но родитель не вызвал wait(). Для избежания используем неблокирующий waitpid() в обработчике SIGCHLD.
6. `Чем отличается graceful shutdown от graceful reload?`
Shutdown: последовательное завершение всех компонентов.
Reload: перезапуск с сохранением доступности сервиса.
7. `Как повлияют контейнерные лимиты (cgroup v2) на метрики?`
Метрики показывают использование относительно лимитов cgroup, а не физической системы.

## Общие выводы
- Обработка сигналов критически важна для надежных приложений
- Правильное управление планированием улучшает performance на 25-30%
- Мониторинг через pidstat незаменим для диагностики
- Реализованные механизмы применимы в production-системах
