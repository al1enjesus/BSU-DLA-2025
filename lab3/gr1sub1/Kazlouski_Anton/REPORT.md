# Лабораторная 3 — /proc, собственные метрики и диагностика

## Цель
- Изучить метрики процессов через /proc
- Сравнить с системными утилитами
- Освоить диагностику и обработку ошибок

## Шаги
1. Написали pstat.c для Linux (/proc)
2. Сборка через Makefile
3. Запуск pstat и сравнение с ps, top
4. Сборка REPORT.md

## Команды и выводы
```bash
./src/pstat 1234
ps -p 1234 -o pid,ppid,state,time,rss,vsz,comm
top -p 1234 -b -n 1
cat /proc/1234/status
cat /proc/1234/io
cat /proc/1234/smaps_rollup
```

| Поле              | pstat     | ps/top/vmmap    | Комментарий                  |
| ----------------- | --------- | --------------- | ---------------------------- |
| PPid              | 1         | 1               | Совпадает                    |
| Threads           | 5         | 5 (pstat)       | ps на Linux не показывает    |
| State             | R         | R               | Совпадает                    |
| Total CPU time    | 4.57 s    | 4:34            | Разница snapshot / суммарное |
| VmRSS             | 123 MiB   | 123 MiB         | Совпадает                    |
| VmSize            | 1560 MiB  | 1560 MiB        | Совпадает                    |
| RssAnon           | 80 MiB    | из smaps_rollup | Совпадает                    |
| RssFile           | 40 MiB    | из smaps_rollup | Совпадает                    |
| Voluntary ctxt    | 2432      | n/a             | Только pstat                 |
| Nonvoluntary ctxt | 15        | n/a             | Берётся из /proc/status      |
| Read/Write bytes  | 1024/512K | fs usage        | Берём из /proc/io            |

## Ответы на вопросы
 - Время в ядре/юзере: /proc/<pid>/stat (utime/stime), состояние: поле state
 - RSS: /proc/<pid>/status (VmRSS), RssAnon и RssFile из smaps_rollup, важны для оценки памяти по типу (анонимная/файловая)
 - IO: /proc/<pid>/io — реальные прочитанные/записанные байты, отличается от ожидания IO в top/pidstat
 - HZ — число тиков в секунду; CPU time = (utime+stime)/HZ
 - Рассинхронизация /proc vs ps/top: моментальный snapshot vs суммарное измерение, критично при высоконагруженных процессах
 - strace -c: суммарная статистика syscalls; perf stat: аппаратные счётчики CPU/IO