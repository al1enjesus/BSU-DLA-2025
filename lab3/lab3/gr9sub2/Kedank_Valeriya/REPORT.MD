# Лабораторная работа №3 — /proc, собственные метрики и диагностика


## Цель

- Научиться извлекать метрики процесса из файлов /proc/<pid>/stat, /status, /io, /smaps_rollup.
- Сравнить показатели с системными утилитами ps, top, pidstat.
- Освоить базовую диагностику: strace, perf.
- Оценить ограничения среды Linux/WSL2 и интерпретировать расхождения.

## C) /proc и собственная утилита pstat

1. Запуск собственной утилиты pstat

```bash
python3 src/pstat.py 2238
```

Вывод:

```bash
Process 2238 (update-manager)
  PPid: 994
  State: S
  Threads: 5
  CPU time: 5.31 sec (utime+stime)
  Context switches: voluntary=1754, nonvoluntary=75
  Memory:
    VmRSS: 193.86 MiB
    RssAnon: 80.90 MiB
    RssFile: 112.46 MiB
  IO:
    read_bytes: 39153664
    write_bytes: 10362880

```

2. Сравнение с ps, top, pidstat

- `ps -p 2238 -o pid,ppid,stat,etime,time,cmd` :


```bash
PID    PPID STAT ELAPSED TIME CMD
2238   994  SNl   33:25   00:00:05 /usr/bin/python3 /usr/bin/update-manager
```

- `top`:

Процесс не попадает в топ по CPU, состояние — S, память RES ≈ 193 MiB.

- `pidstat -p 2238 1 5`:

```bash
Среднее: %CPU = 0.00%
```

## E*) Диагностика и профилирование (со звёздочкой)

1. Диагностика strace

`strace -f -c -p 2238`

Вывод:
`Операция не позволена — ptrace заблокирован (yama/ptrace_scope=1)`

2. Диагностика perf

`perf stat -p 2238 sleep 5`

Вывод:

```bash
<not counted> — события не собраны
perf_event_paranoid = 4 — доступ ограничен
```

Требуется изменение параметра или запуск от root

## Ответы на вопросы

1. Где в /proc/<pid>/stat и /proc/<pid>/status отражаются время в ядре/в юзере и состояние процесса?

- stat: поля utime, stime, state
- status: поле State, Threads, voluntary_ctxt_switches

2. Как получить RSS и чем отличаются RssAnon и RssFile? Почему они важны?

- VmRSS — общий объём резидентной памяти
- RssAnon — анонимные страницы (heap, stack)
- RssFile — отображённые файлы (библиотеки, mmap)

3. Как оценить IO‑активность по /proc/<pid>/io и чем она отличается от «ожидания IO» в top/pidstat?

- read_bytes, write_bytes — фактические байты
- В top/pidstat — IO wait, но не объём

4. Что означает делитель HZ и как корректно посчитать CPU time sec = (utime+stime)/HZ?

- HZ = sysconf(_SC_CLK_TCK)
- CPU time = (utime + stime) / HZ

5. Почему возможны рассинхронизации между /proc и выводом ps/top? Когда это критично?

- /proc — моментальный снимок
- ps/top — периодические обновления
- Критично при высоких нагрузках или коротких процессах

6. Что показывает strace -c и как интерпретировать perf stat?

- strace -c — частота системных вызовов
- perf stat — аппаратные метрики: cycles, instructions, branches
- В данной среде — недоступно без root

## Выводы

- Утилита pstat.py корректно извлекает метрики из /proc.
- Значения совпадают с ps, top, pidstat.
- strace и perf ограничены настройками безопасности.
- Форматирование памяти и CPU времени реализовано.
- Cреда — пользовательская Ubuntu, без root, с ограничениями ptrace и perf.
