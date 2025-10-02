# Лабораторная работа №3 — `/proc`, метрики и диагностика

## Автор
Пардаев Алексей, гр. `9sub1`

## Описание
Реализована утилита **`pstat <pid>`** на Python для анализа процессов через `/proc`.  
Выводит: `PPid`, `Threads`, `State`, `utime/stime`, `CPU time sec`, `VmRSS`, `RssAnon`, `RssFile`, `voluntary_ctxt_switches`, `nonvoluntary_ctxt_switches`, `read_bytes`, `write_bytes`.  
Данные сопоставлены с `ps`, `top`, `pidstat`. Для диагностики применяются `strace` и `perf`.

## Требования
- Ubuntu Linux (20.04+), Python ≥ 3.8  

## Запуск
```bash
cd src
chmod +x pstat.py
./pstat.py <pid>
```