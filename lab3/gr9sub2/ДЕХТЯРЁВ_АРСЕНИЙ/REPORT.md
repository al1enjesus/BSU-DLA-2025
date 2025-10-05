# Лабораторная 3 — /proc, собственные метрики и диагностика

## Цели
- Уверенно извлекать и интерпретировать метрики процесса из `/proc`.
- Сопоставлять показатели `/proc` с утилитами `ps`, `top`, `pidstat`.
- Освоить базовую диагностику приложений: системные вызовы и аппаратные счётчики.

## Задание C /proc и собственная утилита pstat (обязательно)

Напишите утилиту `pstat <pid>`, которая:
- Читает `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup` (если есть).
- Выводит краткую сводку: `PPid`, `Threads`, `State`, `utime/stime`, `voluntary_ctxt_switches`, `nonvoluntary_ctxt_switches`, `VmRSS`, `RssAnon`, `RssFile`, `read_bytes`, `write_bytes`.
- Умеет форматировать числа (МиБ/КиБ) и считает `RSS MiB` и «CPU time sec = (utime+stime)/HZ».

Сравните показания с `ps`, `pidstat`, `top` в отчёте.

## Использование ИИ

ИИ использовался для консультации по содержанию файлов `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup`.  
Также ИИ проводил code review и помог с написанием функции для подсчёта CPU time, объяснив как оно высчитывается и написав команду для получения CLK_TCK.  
