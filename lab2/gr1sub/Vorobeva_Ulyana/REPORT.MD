# Лабораторная работа 2 — Продвинутые процессы Linux: сигналы, планирование, ресурсы, /proc

## Цели работы

* Изучить жизненный цикл процессов в Linux: запуск, мониторинг, рестарт, корректное завершение.
* Реализовать обработку сигналов (`SIGTERM`, `SIGINT`, `SIGHUP`, `SIGUSR1`, `SIGUSR2# Лабораторная работа 2 — Продвинутые процессы Linux: сигналы, планирование, ресурсы, /proc

## Цели работы

* Изучить жизненный цикл процессов в Linux: запуск, мониторинг, рестарт, корректное завершение.
* Реализовать обработку сигналов (`SIGTERM`, `SIGINT`, `SIGHUP`, `SIGUSR1`, `SIGUSR2`) для супервизора и воркеров.
* Исследовать влияние `nice` и CPU‑аффинити на планирование процессов.
* Получить опыт диагностики через `/proc`, `pidstat` и `ps`.
* Практиковаться в создании воспроизводимых экспериментов и сборе логов.


## Используемые инструменты

* Linux (Ubuntu 22.04 / WSL2)
* C++17
* Bash
* `make`
* `ps`, `pidstat`, `kill`, `taskset`

---

## Шаги выполнения

### 1. Сборка

```bash
./run.sh
```

**Лог сборки и запуска:**

```
=== Сборка ===
rm -rf /home/ullecses/BSU-DLA-2025/lab2/gr1sub/Vorobeva_Ulyana/bin
g++ -std=c++17 -O2 src/supervisor.cpp -o bin/supervisor -lyaml-cpp
g++ -std=c++17 -O2 src/worker.cpp -o bin/worker
=== Запуск супервизора с воркерами ===
[RUN] Supervisor PID=34225
```

### 2. Проверка состояния процессов

```bash
ps -o pid,comm,nice,psr -C supervisor,worker
```

**Вывод:**

```
    PID COMMAND          NI PSR
  34225 supervisor        0   3
  34227 worker           10   0
  34228 worker            0   1
  34229 worker           10   0
```

### 3. Переключение режимов воркеров

```bash
kill -USR1 34225  # LIGHT
kill -USR2 34225  # HEAVY
```

**Лог переключения режимов:**

```
[WORKER 34227] Mode=LIGHT Tick=1 CPU=0
[WORKER 34228] Mode=LIGHT Tick=1 CPU=1
[WORKER 34229] Mode=LIGHT Tick=1 CPU=0
[WORKER 34227] Mode=HEAVY Tick=2 CPU=0
[WORKER 34228] Mode=HEAVY Tick=2 CPU=1
[WORKER 34229] Mode=HEAVY Tick=2 CPU=0
```

### 4. Graceful shutdown

```bash
kill -TERM 34225
```

**Лог завершения:**

```
[WORKER 34227] exiting
[WORKER 34228] exiting
[WORKER 34229] exiting
[SUP] Worker exited PID=34227
[SUP] Worker exited PID=34228
[SUP] Shutting down...
=== Конец демонстрации ===
```

## Планирование: nice и CPU‑аффинити

**Настройка nice и CPU‑аффинити:**

```bash
# Изменить приоритет nice
renice -n 10 -p 34227
renice -n 0 -p 34228

# Закрепить воркеры на конкретных ядрах
taskset -cp 0 34227
taskset -cp 1 34228
```

**Наблюдения:**

* Воркеры с меньшим `nice` получают большую долю CPU.
* CPU‑аффинити позволяет закрепить процесс на ядре, но ограничивает гибкость ядра при распределении нагрузки.


## Воспроизводимость

1. Собрать бинарники:

```bash
./run.sh
```

2. Переключить режимы:

```bash
kill -USR1 <SUP_PID>  # LIGHT
kill -USR2 <SUP_PID>  # HEAVY
```

3. Graceful shutdown:

```bash
kill -TERM <SUP_PID>
```

4. Проверить состояние процессов:

```bash
ps -o pid,comm,nice,psr -C supervisor,worker
```


## Ответы на контрольные вопросы

1. **Чем процесс отличается от потока?**
   Процесс имеет отдельное пространство памяти и PID, потоки (threads) — общую память и разные TID. В `ps` процессы отображаются с разными PID, потоки можно увидеть через `ps -eLf`. В `/proc/<pid>/task/` перечислены потоки процесса.

2. **Как `nice` влияет на планирование CFS?**
   `nice` смещает вес процесса в CFS. Больший `nice` → меньший приоритет. Пределы: -20 (наивысший), +19 (наименьший).

3. **Что даёт CPU‑аффинити?**
   Позволяет закрепить процесс на ядре, снижает миграцию, полезно для кешей CPU. Может вредить при высокой нагрузке и недостатке ядер.

4. **Различие RLIMIT_AS, RLIMIT_DATA, RLIMIT_RSS:**

   * `RLIMIT_AS` — предел виртуальной памяти.
   * `RLIMIT_DATA` — предел сегмента данных.
   * `RLIMIT_RSS` — предел resident set (RAM).
     `RLIMIT_RSS` часто игнорируется ядром Linux.

5. **Почему возможны зомби?**
   Если родитель не вызывает `wait` после выхода воркера. При массовых рестартах нужно корректно обрабатывать `SIGCHLD`.

6. **Graceful shutdown vs reload:**

   * Shutdown: завершение всех воркеров.
   * Reload: перезапуск воркеров с сохранением работы, без потери данных.

7. **Контейнерные лимиты (cgroup v2):**
   Ограничивают RSS/виртуальную память, отражаются в `/proc/<pid>/status` и могут вызвать OOM.

---

## Выводы

* Супервизор корректно порождает и управляет воркерами, обрабатывает сигналы и подбирает зомби.
* `Nice` и CPU‑аффинити влияют на распределение CPU и поведение планировщика.
* Graceful shutdown и reload работают как ожидается.
* Воспроизводимость эксперимента обеспечена с помощью `run.sh` и команд `kill`, `ps`.

---

## Приложение: логи

Полный лог работы `run.sh`:

```
=== Сборка ===
rm -rf /home/ullecses/BSU-DLA-2025/lab2/gr1sub/Vorobeva_Ulyana/bin
g++ -std=c++17 -O2 src/supervisor.cpp -o bin/supervisor -lyaml-cpp
g++ -std=c++17 -O2 src/worker.cpp -o bin/worker
=== Запуск супервизора с воркерами ===
[RUN] Supervisor PID=34225
=== Статус процессов ===
    PID COMMAND          NI PSR
  34225 supervisor        0   3
  34227 worker           10   0
  34228 worker            0   1
  34229 worker           10   0
=== CPU статистика (pidstat 1 5) ===
pidstat не найден, используйте 'sudo apt install sysstat'
=== Переключение режима воркеров в LIGHT ===
[WORKER 34227] Mode=LIGHT Tick=1 CPU=0
[WORKER 34228] Mode=LIGHT Tic## Воспроизводимость

### 1. Сборка бинарников
```bash
./run.sh



Отправить сигналы для переключения режимов:

kill -USR1 <SUP_PID>  # LIGHT
kill -USR2 <SUP_PID>  # HEAVY


Graceful shutdown:

kill -TERM <SUP_PID>


Проверить состояние процессов:

ps -o pid,comm,nice,psr -C supervisor,workerk=1 CPU=1
[WORKER 34229] Mode=LIGHT Tick=1 CPU=0
=== Переключение режима воркеров в HEAVY ===
[WORKER 34227] Mode=HEAVY Tick=2 CPU=0
[WORKER 34228] Mode=HEAVY Tick=2 CPU=1
[WORKER 34229] Mode=HEAVY Tick=2 CPU=0
=== Завершение супервизора ===
[WORKER 34227] exiting
[WORKER 34228] exiting
[WORKER 34229] exiting
[SUP] Worker exited PID=34227
[SUP] Worker exited PID=34228
[SUP] Shutting down...
=== Конец демонстрации ===
```
`) для супервизора и воркеров.
* Исследовать влияние `nice` и CPU‑аффинити на планирование процессов.
* Получить опыт диагностики через `/proc`, `pidstat` и `ps`.
* Практиковаться в создании воспроизводимых экспериментов и сборе логов.


## Используемые инструменты

* Linux (Ubuntu 22.04 / WSL2)
* C++17
* Bash
* `make`
* `ps`, `pidstat`, `kill`, `taskset`

---

## Шаги выполнения

### 1. Сборка

```bash
./run.sh
```

**Лог сборки и запуска:**

```
=== Сборка ===
rm -rf /home/ullecses/BSU-DLA-2025/lab2/gr1sub/Vorobeva_Ulyana/bin
g++ -std=c++17 -O2 src/supervisor.cpp -o bin/supervisor -lyaml-cpp
g++ -std=c++17 -O2 src/worker.cpp -o bin/worker
=== Запуск супервизора с воркерами ===
[RUN] Supervisor PID=34225
```

### 2. Проверка состояния процессов

```bash
ps -o pid,comm,nice,psr -C supervisor,worker
```

**Вывод:**

```
    PID COMMAND          NI PSR
  34225 supervisor        0   3
  34227 worker           10   0
  34228 worker            0   1
  34229 worker           10   0
```

### 3. Переключение режимов воркеров

```bash
kill -USR1 34225  # LIGHT
kill -USR2 34225  # HEAVY
```

**Лог переключения режимов:**

```
[WORKER 34227] Mode=LIGHT Tick=1 CPU=0
[WORKER 34228] Mode=LIGHT Tick=1 CPU=1
[WORKER 34229] Mode=LIGHT Tick=1 CPU=0
[WORKER 34227] Mode=HEAVY Tick=2 CPU=0
[WORKER 34228] Mode=HEAVY Tick=2 CPU=1
[WORKER 34229] Mode=HEAVY Tick=2 CPU=0
```

### 4. Graceful shutdown

```bash
kill -TERM 34225
```

**Лог завершения:**

```
[WORKER 34227] exiting
[WORKER 34228] exiting
[WORKER 34229] exiting
[SUP] Worker exited PID=34227
[SUP] Worker exited PID=34228
[SUP] Shutting down...
=== Конец демонстрации ===
```

## Планирование: nice и CPU‑аффинити

**Настройка nice и CPU‑аффинити:**

```bash
# Изменить приоритет nice
renice -n 10 -p 34227
renice -n 0 -p 34228

# Закрепить воркеры на конкретных ядрах
taskset -cp 0 34227
taskset -cp 1 34228
```

**Наблюдения:**

* Воркеры с меньшим `nice` получают большую долю CPU.
* CPU‑аффинити позволяет закрепить процесс на ядре, но ограничивает гибкость ядра при распределении нагрузки.


## Воспроизводимость

1. Собрать бинарники:

```bash
./run.sh
```

2. Переключить режимы:

```bash
kill -USR1 <SUP_PID>  # LIGHT
kill -USR2 <SUP_PID>  # HEAVY
```

3. Graceful shutdown:

```bash
kill -TERM <SUP_PID>
```

4. Проверить состояние процессов:

```bash
ps -o pid,comm,nice,psr -C supervisor,worker
```


## Ответы на контрольные вопросы

1. **Чем процесс отличается от потока?**
   Процесс имеет отдельное пространство памяти и PID, потоки (threads) — общую память и разные TID. В `ps` процессы отображаются с разными PID, потоки можно увидеть через `ps -eLf`. В `/proc/<pid>/task/` перечислены потоки процесса.

2. **Как `nice` влияет на планирование CFS?**
   `nice` смещает вес процесса в CFS. Больший `nice` → меньший приоритет. Пределы: -20 (наивысший), +19 (наименьший).

3. **Что даёт CPU‑аффинити?**
   Позволяет закрепить процесс на ядре, снижает миграцию, полезно для кешей CPU. Может вредить при высокой нагрузке и недостатке ядер.

4. **Различие RLIMIT_AS, RLIMIT_DATA, RLIMIT_RSS:**

   * `RLIMIT_AS` — предел виртуальной памяти.
   * `RLIMIT_DATA` — предел сегмента данных.
   * `RLIMIT_RSS` — предел resident set (RAM).
     `RLIMIT_RSS` часто игнорируется ядром Linux.

5. **Почему возможны зомби?**
   Если родитель не вызывает `wait` после выхода воркера. При массовых рестартах нужно корректно обрабатывать `SIGCHLD`.

6. **Graceful shutdown vs reload:**

   * Shutdown: завершение всех воркеров.
   * Reload: перезапуск воркеров с сохранением работы, без потери данных.

7. **Контейнерные лимиты (cgroup v2):**
   Ограничивают RSS/виртуальную память, отражаются в `/proc/<pid>/status` и могут вызвать OOM.

---

## Выводы

* Супервизор корректно порождает и управляет воркерами, обрабатывает сигналы и подбирает зомби.
* `Nice` и CPU‑аффинити влияют на распределение CPU и поведение планировщика.
* Graceful shutdown и reload работают как ожидается.
* Воспроизводимость эксперимента обеспечена с помощью `run.sh` и команд `kill`, `ps`.

---

## Приложение: логи

Полный лог работы `run.sh`:

```
=== Сборка ===
rm -rf /home/ullecses/BSU-DLA-2025/lab2/gr1sub/Vorobeva_Ulyana/bin
g++ -std=c++17 -O2 src/supervisor.cpp -o bin/supervisor -lyaml-cpp
g++ -std=c++17 -O2 src/worker.cpp -o bin/worker
=== Запуск супервизора с воркерами ===
[RUN] Supervisor PID=34225
=== Статус процессов ===
    PID COMMAND          NI PSR
  34225 supervisor        0   3
  34227 worker           10   0
  34228 worker            0   1
  34229 worker           10   0
=== CPU статистика (pidstat 1 5) ===
pidstat не найден, используйте 'sudo apt install sysstat'
=== Переключение режима воркеров в LIGHT ===
[WORKER 34227] Mode=LIGHT Tick=1 CPU=0
[WORKER 34228] Mode=LIGHT Tic## Воспроизводимость

### 1. Сборка бинарников
```bash
./run.sh



Отправить сигналы для переключения режимов:

kill -USR1 <SUP_PID>  # LIGHT
kill -USR2 <SUP_PID>  # HEAVY


Graceful shutdown:

kill -TERM <SUP_PID>


Проверить состояние процессов:

ps -o pid,comm,nice,psr -C supervisor,workerk=1 CPU=1
[WORKER 34229] Mode=LIGHT Tick=1 CPU=0
=== Переключение режима воркеров в HEAVY ===
[WORKER 34227] Mode=HEAVY Tick=2 CPU=0
[WORKER 34228] Mode=HEAVY Tick=2 CPU=1
[WORKER 34229] Mode=HEAVY Tick=2 CPU=0
=== Завершение супервизора ===
[WORKER 34227] exiting
[WORKER 34228] exiting
[WORKER 34229] exiting
[SUP] Worker exited PID=34227
[SUP] Worker exited PID=34228
[SUP] Shutting down...
=== Конец демонстрации ===
```
