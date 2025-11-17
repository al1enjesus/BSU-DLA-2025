# Лабораторная 2 — Продвинутые процессы Linux: сигналы, планирование, ресурсы, /proc

## Цели
- Уверенно работать с жизненным циклом процессов: запуск, рестарт, корректное завершение.
- Реализовать обработку сигналов и протокол «graceful shutdown / reload».
- Управлять планированием: nice/приоритеты, CPU‑аффинити.
- Снимать и интерпретировать метрики из `/proc` и системных утилит.
- Исследовать влияние ограничений ресурсов (rlimits, опционально cgroup v2).

### A) Мини‑супервизор с воркерами
Реализовать процесс‑родителя (супервизор), который:
- Порождает N воркеров (N ≥ 2), отслеживает их состояние через `SIGCHLD`, корректно «подбирает» зомби.
- Поддерживает сигналы:
  - `SIGTERM`/`SIGINT`: корректное завершение — послать воркерам «стоп» и дождаться их выхода (graceful shutdown ≤ 5 секунд).
  - `SIGHUP`: «graceful reload» — перечитать конфиг и «мягко» перезапустить воркеров без потери работы (допускается короткая пауза).
  - `SIGUSR1`: широковещательно переключить воркеров в «лёгкий» режим нагрузки, `SIGUSR2` — обратно в «тяжёлый».
- При аварийном выходе воркера — перезапускает его (ограничить частоту рестартов, например не более 5 за 30 секунд).

Требования к воркерам:
- Воркеры выполняют имитацию работы: период «вычислений» + период ожидания, параметры берутся из конфига.
- Воркеры обрабатывают `SIGTERM` (корректное завершение) и `SIGUSR1/2` (переключение профиля нагрузки).
- Выводят диагностические строки: PID, режим работы, тик‑статистику, на какой(их) CPU исполняются.


#### Сборка
В каталоге `src/` выполнить:
```bash
make
```
Пересборка:
```bash
make clean && make
```

#### Конфигурация

src/config.cfg:
```
workers=3
initial_mode=heavy
work_us_heavy=9000
sleep_us_heavy=1000
work_us_light=2000
sleep_us_light=8000
```

workers — количество воркеров;
initial_mode — начальный режим (heavy или light);
work_us_* / sleep_us_* — эмуляция работы/паузы в микросекундах.

#### Запуск 
Рекомендуемый запуск (универсальный):
```
./supervisor
```

#### Демонстрация действий и комментарии с выводами

##### Запуск супервизора

Команда:
```
./supervisor
```

Фрагменты логов:
```
[SUP] config loaded: workers=3 initial_mode=heavy
[SUP] started worker id=0 pid=6242
[SUP] started worker id=1 pid=6243
[SUP] started worker id=2 pid=6244
[SUP] monitor loop enter./supervisor
```

Супервизор загрузил конфиг, запустил 3 воркера и вошёл в основной цикл. PID воркеров нужно зафиксировать для дальнейших операций.

##### Переключение режимов — SIGUSR1 (LIGHT)

Команда:
```
kill -USR1 <supervisor_pid>
```

Фрагменты логов:
```
[SUP] SIGUSR1 -> broadcasting to workers
[WORK 6375] switched to LIGHT
[WORK 6377] switched to LIGHT
[WORK 6376] switched to LIGHT
```

Вывод:
Супервизор рассылкой пересылает сигнал воркерам; воркеры меняют internal mode на light и начинают работать по параметрам mode_light (меньше work_us, больше sleep_us) — CPU-активность уменьшается.
##### Переключение режимов — SIGUSR2 (HEAVY)

Команда:
```
kill -USR2 <supervisor_pid>
```
Фрагменты логов:
```
[SUP] SIGUSR2 -> broadcasting to workers
[WORK 6376] switched to HEAVY
[WORK 6375] switched to HEAVY
[WORK 6377] switched to HEAVY
```
Вывод:
Аналогично: воркеры переключаются в heavy — дольше заняты, выше %CPU.
##### Симуляция краша воркера

Команды:

Найти pid воркеров(в текущей конфигурации 3 воркера => head -n3):
```    
ps --no-headers -o pid --ppid <supervisor_pid> | head -n3
```

Убить одного из них:
```
kill -9 <worker_pid>
```

Фрагменты логов супервизора:
```
[SUP] reaped pid=6702 exit=-1
[SUP] restarting worker id=2
[SUP] started worker id=2 pid=6788
```

Вывод:
Супервизор реапит (reap) завершившийся дочерний процесс и перезапускает его. Поведение регулируется ограничением рестартов (не чаще MAX_RESTARTS за RESTART_WINDOW).

##### Перезагрузка конфигурации — SIGHUP (graceful reload)

Команда:
```
kill -HUP <supervisor_pid>
```

Фрагмент логов:
```
[SUP] SIGHUP received -> reload config and replace workers
[SUP] config loaded: workers=3 initial_mode=heavy
[SUP] starting replacement for worker 0
[SUP] started worker id=0 pid=7932
[SUP] terminated old worker pid=7925
[SUP] starting replacement for worker 1
[SUP] started worker id=1 pid=7933
[SUP] terminated old worker pid=7926
[SUP] starting replacement for worker 2
[SUP] started worker id=2 pid=7934
[SUP] terminated old worker pid=7927
[WORK 7932] config loaded
[WORK 7933] config loaded
[WORK 7934] config loaded
[WORK 7927] graceful exit (ticks=2004)
[WORK 7925] graceful exit (ticks=2004)
[WORK 7926] graceful exit (ticks=2004)
[SUP] reaped pid=7925 exit=0
[SUP] unknown child pid=7925 reaped
[SUP] reaped pid=7926 exit=0
[SUP] unknown child pid=7926 reaped
[SUP] reaped pid=7927 exit=0
[SUP] unknown child pid=7927 reaped
```

Вывод:
По моей реализации супервизор стартует заменяющие процессы, затем завершает старые (overlap), чтобы избежать потери задач. Это простая форма грейсфул перезапуска.

##### Graceful shutdown — SIGTERM

Команда:
```
kill -SIGTERM <supervisor_pid> (или CTRL-C в терминале, где запущен supervisor)
```

Логи:
```
[SUP] SIGTERM/SIGINT received -> graceful shutdown
[SUP] sent signal 15 to pid=7932
[SUP] sent signal 15 to pid=7933
[SUP] sent signal 15 to pid=7934
[WORK 7934] mode=heavy tick=30535 pid=7934 affinity=1
[WORK 7933] mode=heavy tick=30535 pid=7933 affinity=1
[WORK 7932] mode=heavy tick=30540 pid=7932 affinity=1
[WORK 7932] graceful exit (ticks=30540)
[WORK 7934] graceful exit (ticks=30535)
[WORK 7933] graceful exit (ticks=30535)
[SUP] sent signal 9 to pid=7932
[SUP] sent signal 9 to pid=7933
[SUP] sent signal 9 to pid=7934
[SUP] shutdown complete
```

Вывод:
Супервизор посылает SIGTERM всем воркерам и ожидает их корректного завершения.

### B) Планирование: nice и CPU‑аффинити
Добавить управление планированием для воркеров:
- Установите разный `nice` для поднабора воркеров (например, половине `+10`, остальным `0`). Сравните распределение CPU.
- Установите CPU‑аффинити (через `sched_setaffinity` или `taskset`) для разных воркеров: например, часть закрепить на 0‑м ядре, часть — на 1‑м.
- Снимите метрики `pidstat -u 1 10` и покажите, как `nice` и аффинити влияют на `%CPU`/время ожидания.

Что сделано
Парсер конфигурации (supervisor.c) дополнен:
nice_pattern — строка с целыми через запятую (напр. 0,10).
affinity_pattern — блоки через |, каждый блок — список CPU через запятую (напр. 0|1|2,3).

Supervisor:
Для каждого worker формирует аргументы --id, --config, --nice, --affinity.
Печатает debug-строку "[SUP-debug] exec args: ..." перед execl, чтобы легко проверить, какие аргументы были переданы.
Реализована рестарт-политика, обработка сигналов (SIGTERM, SIGHUP, SIGUSR1, SIGUSR2), graceful reload.

Worker:
При старте парсит --nice и --affinity.
Вызывает setpriority(PRIO_PROCESS, 0, nice) и sched_setaffinity(0, ...).
Печатает в stdout/stderr подтверждение: "[WORK <pid>] set nice=X (applied)" и "[WORK <pid>] applied affinity=A,B" или ошибку.
Реализует режимы work/sleep (light/heavy), реагирует на сигналы.
Дополнительно добавлены run.sh (демонстрация) и test_affinity.c (микротест для sched_setaffinity).

Формат конфигурации (src/config.cfg)
Пример:
```
workers=4
initial_mode=heavy
work_us_heavy=9000
sleep_us_heavy=1000
work_us_light=2000
sleep_us_light=8000
nice_pattern=0,10
affinity_pattern=0|1|2,3
```

Эксперимент
Сборка
```
cd src
make clean && make
```

Запуск супервизора
```
./supervisor
```
Логи:
```
[SUP] config loaded: workers=4 initial_mode=heavy
[SUP] nice pattern: 0 10
[SUP] affinity pattern parsed, blocks=3
[SUP] started worker id=0 pid=7339
[SUP-debug] exec args: ./worker --id 0 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 0 --affinity 0
[SUP] started worker id=1 pid=7340
[SUP-debug] exec args: ./worker --id 1 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 10 --affinity 1
[SUP] started worker id=2 pid=7341
[SUP-debug] exec args: ./worker --id 2 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 0 --affinity 2,3
[SUP] started worker id=3 pid=7342
[SUP] monitor loop enter
[SUP-debug] exec args: ./worker --id 3 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 10 --affinity 0
[WORK 7339] set nice=0 (applied)
[WORK 7340] set nice=10 (applied)
[WORK 7339] applied affinity=0
[WORK 7340] applied affinity=1
[WORK 7339] config loaded
[WORK 7340] config loaded
[WORK 7341] set nice=0 (applied)
[WORK 7342] set nice=10 (applied)
[WORK 7341] applied affinity=2,3
[WORK 7341] config loaded
[WORK 7342] applied affinity=0
[WORK 7342] config loaded
```

Замер загрузки (pidstat)

Снимаем pidstat 5 раз с интервалом 1s:
```
pidstat -u -p 7339,7340,7341,7342 1 5 > pidstat_B.txt
```

pidstat_B.txt:
```
13:24:50      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
13:24:51     1000      7339    6,00   68,00    0,00   15,00   74,00     0  worker
13:24:51     1000      7340    8,00   83,00    0,00    0,00   91,00     1  worker
13:24:51     1000      7341    6,00   84,00    0,00    0,00   90,00     3  worker
13:24:51     1000      7342    2,00   18,00    0,00   74,00   20,00     0  worker

13:24:51      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
13:24:52     1000      7339    6,00   69,00    0,00   14,00   75,00     0  worker
13:24:52     1000      7340    7,00   81,00    0,00    0,00   88,00     1  worker
13:24:52     1000      7341    5,00   84,00    0,00    0,00   89,00     3  worker
13:24:52     1000      7342    2,00   18,00    0,00   75,00   20,00     0  worker

13:24:52      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
13:24:53     1000      7339    5,00   70,00    0,00   16,00   75,00     0  worker
13:24:53     1000      7340    8,00   83,00    0,00    0,00   91,00     1  worker
13:24:53     1000      7341    4,00   85,00    0,00    0,00   89,00     3  worker
13:24:53     1000      7342    1,00   20,00    0,00   74,00   21,00     0  worker

13:24:53      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
13:24:54     1000      7339    6,00   69,00    0,00   14,00   75,00     0  worker
13:24:54     1000      7340    7,00   82,00    0,00    0,00   89,00     1  worker
13:24:54     1000      7341    6,00   84,00    0,00    0,00   90,00     3  worker
13:24:54     1000      7342    1,00   18,00    0,00   75,00   19,00     0  worker

13:24:54      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
13:24:55     1000      7339    6,00   68,00    0,00   15,00   74,00     0  worker
13:24:55     1000      7340    7,00   82,00    0,00    0,00   89,00     1  worker
13:24:55     1000      7341    4,00   85,00    0,00    0,00   89,00     3  worker
13:24:55     1000      7342    1,00   19,00    0,00   75,00   20,00     0  worker

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:     1000      7339    5,80   68,80    0,00   14,80   74,60     -  worker
Average:     1000      7340    7,40   82,20    0,00    0,00   89,60     -  worker
Average:     1000      7341    5,00   84,40    0,00    0,00   89,40     -  worker
Average:     1000      7342    1,40   18,60    0,00   74,60   20,00     -  worker
```

Ниже пример с lightmode и каждый процесс выполняется на отдельном ядре.
```
14:56:14      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
14:56:15        0      9385    0,99   17,82    0,00    0,00   18,81     0  worker
14:56:15        0      9386    0,00   18,81    0,00    0,00   18,81     1  worker
14:56:15        0      9387    1,98   18,81    0,00    0,00   20,79     2  worker
14:56:15        0      9388    0,99   18,81    0,00    0,00   19,80     3  worker

14:56:15      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
14:56:16        0      9385    3,00   18,00    0,00    0,00   21,00     0  worker
14:56:16        0      9386    2,00   19,00    0,00    0,00   21,00     1  worker
14:56:16        0      9387    2,00   18,00    0,00    1,00   20,00     2  worker
14:56:16        0      9388    3,00   18,00    0,00    0,00   21,00     3  worker

14:56:16      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
14:56:17        0      9385    1,00   19,00    0,00    0,00   20,00     0  worker
14:56:17        0      9386    2,00   19,00    0,00    0,00   21,00     1  worker
14:56:17        0      9387    2,00   18,00    0,00    0,00   20,00     2  worker
14:56:17        0      9388    1,00   19,00    0,00    0,00   20,00     3  worker

14:56:17      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
14:56:18        0      9385    2,00   18,00    0,00    0,00   20,00     0  worker
14:56:18        0      9386    2,00   18,00    0,00    0,00   20,00     1  worker
14:56:18        0      9387    1,00   19,00    0,00    0,00   20,00     2  worker
14:56:18        0      9388    2,00   18,00    0,00    0,00   20,00     3  worker

14:56:18      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
14:56:19        0      9385    2,00   19,00    0,00    0,00   21,00     0  worker
14:56:19        0      9386    2,00   18,00    0,00    0,00   20,00     1  worker
14:56:19        0      9387    2,00   18,00    0,00    0,00   20,00     2  worker
14:56:19        0      9388    1,00   18,00    0,00    0,00   19,00     3  worker

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      9385    1,80   18,36    0,00    0,00   20,16     -  worker
Average:        0      9386    1,60   18,56    0,00    0,00   20,16     -  worker
Average:        0      9387    1,80   18,36    0,00    0,20   20,16     -  worker
Average:        0      9388    1,60   18,36    0,00    0,00   19,96     -  worker
```

С 4 процессами, распределёнными по 2 ядрам:
```
[SUP] config loaded: workers=4 initial_mode=heavy
[SUP] nice pattern: 0 10
[SUP] affinity pattern parsed, blocks=2
[SUP] started worker id=0 pid=9752
[SUP-debug] exec args: ./worker --id 0 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 0 --affinity 0
[SUP] started worker id=1 pid=9753
[SUP-debug] exec args: ./worker --id 1 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 10 --affinity 1
[SUP] started worker id=2 pid=9754
[SUP-debug] exec args: ./worker --id 2 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 0 --affinity 0
[SUP] started worker id=3 pid=9755
[SUP] monitor loop enter
[SUP-debug] exec args: ./worker --id 3 --config /home/vanya/BSU-DLA-2025/lab2/gr1sub1/Tsybenka_Kirill/src/config.cfg --nice 10 --affinity 1
[WORK 9752] set nice=0 (applied)
[WORK 9752] applied affinity=0
[WORK 9754] set nice=0 (applied)
[WORK 9752] config loaded
[WORK 9753] set nice=10 (applied)
[WORK 9755] set nice=10 (applied)
[WORK 9753] applied affinity=1
[WORK 9753] config loaded
[WORK 9754] applied affinity=0
[WORK 9755] applied affinity=1
[WORK 9755] config loaded
[WORK 9754] config loaded
[WORK 9752] mode=heavy tick=5 pid=9752 nice=0 [WORK 9752] affinity:0

```

Результат команды pidstat -u -p 9752,9753,9754,9755 1 5 > pidstat_B.txt:
```
15:14:39      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:14:40        0      9752    3,00   45,00    0,00   43,00   48,00     0  worker
15:14:40        0      9753    4,00   46,00    0,00   41,00   50,00     1  worker
15:14:40        0      9754    4,00   43,00    0,00   44,00   47,00     0  worker
15:14:40        0      9755    3,00   46,00    0,00   41,00   49,00     1  worker

15:14:40      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:14:41        0      9752    4,00   46,00    0,00   42,00   50,00     0  worker
15:14:41        0      9753    3,00   47,00    0,00   42,00   50,00     1  worker
15:14:41        0      9754    3,00   46,00    0,00   41,00   49,00     0  worker
15:14:41        0      9755    4,00   47,00    0,00   41,00   51,00     1  worker

15:14:41      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:14:42        0      9752    3,00   47,00    0,00   41,00   50,00     0  worker
15:14:42        0      9753    3,00   47,00    0,00   41,00   50,00     1  worker
15:14:42        0      9754    3,00   47,00    0,00   42,00   50,00     0  worker
15:14:42        0      9755    4,00   46,00    0,00   41,00   50,00     1  worker

15:14:42      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:14:43        0      9752    4,00   43,00    0,00   44,00   47,00     0  worker
15:14:43        0      9753    3,00   47,00    0,00   41,00   50,00     1  worker
15:14:43        0      9754    4,00   43,00    0,00   45,00   47,00     0  worker
15:14:43        0      9755    4,00   45,00    0,00   42,00   49,00     1  worker

15:14:43      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:14:44        0      9752    3,00   44,00    0,00   45,00   47,00     0  worker
15:14:44        0      9753    3,00   47,00    0,00   41,00   50,00     1  worker
15:14:44        0      9754    4,00   42,00    0,00   46,00   46,00     0  worker
15:14:44        0      9755    4,00   47,00    0,00   41,00   51,00     1  worker

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      9752    3,40   45,00    0,00   43,00   48,40     -  worker
Average:        0      9753    3,20   46,80    0,00   41,20   50,00     -  worker
Average:        0      9754    3,60   44,20    0,00   43,60   47,80     -  worker
Average:        0      9755    3,80   46,20    0,00   41,20   50,00     -  worker
```

Вывод:
Процент занятия ядер процессами с разными nice не сильно отличается.


## Вопросы

**1. Чем процесс отличается от потока в Linux? Где это видно в `ps` и `/proc`?**
В Linux процесс — это единица управления ресурсами (PID, адресное пространство, открытые файловые дескрипторы), а поток (thread) — это «лёгкий процесс» (LWP), который разделяет ресурсы процесса, но имеет собственный TID (thread ID).
В `ps` можно увидеть потоки с ключами `-L` или `-T` (они отображаются как LWP).
В `/proc` у процесса есть каталог `/proc/PID/`, а у потоков внутри процесса — подкаталоги `/proc/PID/task/TID/`.

**2. Как `nice` влияет на планирование CFS? Какие есть пределы/исключения?**
Планировщик CFS (Completely Fair Scheduler) использует параметр `nice` для задания относительного приоритета процесса. Чем выше `nice` (например, `+19`), тем меньше вес процесса и тем реже он получает процессорное время. Чем ниже `nice` (например, `-20`), тем чаще процесс планируется.
Пределы: `-20` (максимальный приоритет) до `+19` (минимальный приоритет).
Исключение: процессы с реальным временем (RT, классы `SCHED_FIFO`, `SCHED_RR`) не подчиняются `nice`.

**3. Что даёт CPU-аффинити и когда она вредна?**
CPU-аффинити фиксирует процесс (или поток) на конкретном ядре или наборе ядер. Это полезно для:
- измерений производительности, чтобы уменьшить миграции между ядрами;
- NUMA-систем, чтобы сократить задержки доступа к памяти;
- высокопроизводительных приложений, где важна предсказуемость. 
Но вред возможен, если:
- ядро перегружено, а остальные простаивают (снижение общей эффективности);
- нарушается балансировка кэшей и NUMA-доступа.

**4. Чем отличаются `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_RSS`? Почему `RLIMIT_RSS` часто игнорируется?**
- `RLIMIT_AS` — ограничение общего виртуального адресного пространства процесса (включает код, данные, mmap). 
- `RLIMIT_DATA` — ограничение сегмента данных (heap, т.е. `brk`/`sbrk`). 
- `RLIMIT_RSS` — ограничение resident set size (фактическая физическая память в RAM).
`RLIMIT_RSS` почти всегда игнорируется в современных ядрах Linux, потому что ядро не может жёстко ограничить RSS без серьёзных накладных расходов. Вместо этого используют cgroups.

**5. Почему возможны зомби и как их избежать при массовых рестартах воркеров?**
Зомби появляются, когда процесс завершился, но его родитель не сделал `wait()` и не забрал код завершения. В системе остаётся запись в таблице процессов.
Избежать можно:
- всегда вызывать `waitpid` в супервизоре;
- ставить обработчик сигнала `SIGCHLD`, который вызывает `waitpid(-1, …, WNOHANG)`;
- использовать опцию `SA_NOCLDWAIT` при установке обработчика `SIGCHLD`.

**6. Чем отличается «graceful shutdown» от «graceful reload/restart»? Какие последовательности безопасны?**
- **Graceful shutdown** — корректное завершение работы: процесс сначала обрабатывает текущие задачи, закрывает ресурсы, затем завершает работу.
- **Graceful reload/restart** — перезапуск с сохранением непрерывности: старые воркеры обслуживают активные запросы, пока новые уже запущены и готовы. После этого старые воркеры завершаются.
Безопасные последовательности:
1. Запустить новые воркеры.
2. Перенаправить им трафик/работу.
3. Дождаться завершения старых (или установить таймаут).
4. Закрыть старые воркеры.

**7. Как повлияют контейнерные лимиты (cgroup v2) на наблюдаемые метрики процесса?**
Контейнеры в Linux используют cgroup v2 для ограничения ресурсов. Это влияет на метрики:
- CPU time будет ограничен квотами/шэдингом (`cpu.max`, `cpu.weight`), и процессы будут получать меньше CPU даже при «свободной» системе.
- Память ограничена через `memory.max`, и процесс при превышении получит OOM-kill.
- Метрики из `/proc` (например, `VmSize`, `VmRSS`) будут показывать потребление внутри контейнера, но общее ограничение определяется cgroup.
- `pidstat`/`top` могут показывать высокую загрузку CPU внутри контейнера, даже если «снаружи» процесс получает только часть ядра.


## Как использовался ИИ
Искусственный интеллект использовался для написания файлов worker.c supervisor.c, для объяснения затруднительных мест.
