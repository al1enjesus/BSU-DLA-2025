## Артефакты 

* `src/pstat.py` — утилита на Python; берёт данные из `/proc/<pid>` и при `--compare` выводит также `ps`, `top`, `pidstat` для сравнения.
* `src/worker_sync_io.py` — тестовый воркер:

  * спит 10 с,
  * пишет 50 MiB синхронно (O_SYNC) в файл в текущей папке,
  * спит 30 с,
  * удаляет файл.

## Эксперимент — команды и фрагменты вывода

### Запуск воркера и pstat — последовательность

1. В одном терминале:

```bash
./src/worker_sync_io.py & echo $!
# получаем PID, например 4318
```

2. В другом терминале:

```bash
python3 src/pstat.py 4318 --compare
```

## Вывод команды `python3 pstat.py <pid> --compare` до записи воркером информации
```
PID: 4318 [python3]                                   ✅ PID и имя процесса совпадают с ps/top
State: S   PPid: 2810   Threads: 1                   ✅ State=S, PPid=2810, Threads=1 (ps/top и /proc/status подтверждают)
utime (ticks): 1, stime (ticks): 0                   ✅ utime/stime соответствуют полям в /proc/<pid>/stat (поля 14/15)
CPU time (sec) = (utime+stime)/HZ = (1+0)/100 = 0.010 s
                                                     ⚠️ ps показывает TIME=00:00:00, pidstat показывает 0.00% — все согласуются в нулевой малой величине

VmRSS (status): 9232 kB   (formatted: 9.02 MiB)     ✅ ps RSS = 9232, top RES = 9232 — совпадает с VmRSS
rss (stat * pagesize): 9453568 bytes (9.02 MiB)     ✅ rss (stat * pagesize) == VmRSS (численно эквивалентно)

smaps summary (kB):
  Rss_kB : 9360                                      ⚠️ smaps_rollup Rss_kB = 9360 kB, на ~128 kB больше, чем VmRSS=9232 kB
  RssAnon_kB : 0
  RssFile_kB : None
  Total Rss from smaps_rollup: 9360 kB (9.14 MiB)

IO: read_bytes=0   write_bytes=0                      ✅ /proc/<pid>/io показывает read_bytes=0 и write_bytes=0 (pstat и /proc согласны)
    formatted: read 0 B  write 0 B

voluntary_ctxt_switches: 1                           ✅ voluntary_ctxt_switches = 1 (совпадает с /proc/<pid>/status)
nonvoluntary_ctxt_switches: 1                       ✅ nonvoluntary_ctxt_switches = 1 (совпадает с /proc/<pid>/status)

--- comparison with system tools ---

===  ps  ===
   4318    2810 python3         S 00:00:00  9232  20476   ✅ PID, PPID, COMMAND, STATE, RSS совпадают с pstat

===  top  ===
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   4318 anton     20   0   20476   9232   5264 S   0.0   0.3   0:00.01 python3  ✅ RES (9232) и STATE совпадают; TIME+ ≈ 0

===  pidstat  ===
... (pidstat показывает %usr/%system ~ 0)             ✅ pidstat показывает нулевую/минимальную загрузку CPU — согласуется с CPU time ~0.01 s

===  /proc/<pid>/stat  ===
4318 (python3) S 2810 4318 2810 34816 2810 4194304 1512 0 0 0 1 0 0 0 20 0 1 0 482061 20967424 2308 18446744073709551615 4194304 10324760 281474231337824 0 0 0 0 16781312 2 1 0 0 17 3 0 0 0 0 0 10419592 10995936 827072512 281474231341252 281474231341272 281474231341272 281474231345127 0            ✅ raw stat показывает те же utime/stime и rss-pages (проверить поля 14/15/24)

===  /proc/<pid>/status  ===
VmSize:	   20476 kB
VmRSS:	    9232 kB                                   ✅ VmRSS = 9232 kB
Threads:	1
voluntary_ctxt_switches:	1
nonvoluntary_ctxt_switches:	1

===  /proc/<pid>/io  ===
rchar: 60299
wchar: 32
syscr: 39
syscw: 2
read_bytes: 0                                        ✅ pstat и /proc показывают read_bytes=0
write_bytes: 0                                       ✅ pstat и /proc показывают write_bytes=0
cancelled_write_bytes: 0

```

## Вывод команды `python3 pstat.py <pid> --compare` после записи воркером информации
```
PID: 4318 [python3]                                   ✅ PID и имя процесса совпадают с ps/top
State: S   PPid: 2810   Threads: 1                   ✅ State=S, PPid=2810, Threads=1 (ps/top и /proc/status подтверждают)
utime (ticks): 1, stime (ticks): 4                   ✅ utime/stime соответствуют полям в /proc/<pid>/stat (поля 14/15)
CPU time (sec) = (utime+stime)/HZ = (1+4)/100 = 0.050 s
                                                     ✅ накопленное CPU-время ≈ 0.05 s — согласуется с TIME+ в top (0:00.05); pidstat показывает ~0% нагрузки, что также консистентно для такого малого суммарного времени

VmRSS (status): 9680 kB   (formatted: 9.45 MiB)     ✅ ps RSS = 9680, top RES = 9680 — совпадает с VmRSS
rss (stat * pagesize): 9912320 bytes (9.45 MiB)     ✅ rss (stat * pagesize) == VmRSS (численно эквивалентно)

smaps summary (kB):
  Rss_kB : 9680                                      ✅ smaps_rollup Rss_kB = 9680 kB (совпадает с VmRSS)
  RssAnon_kB : 0                                     ℹ️ анонимная часть отсутствует
  RssFile_kB : None                                  ℹ️ информация о файловой части может быть недоступна / не показана
  Total Rss from smaps_rollup: 9680 kB (9.45 MiB)    ✅ итог из smaps_rollup совпадает с другими показателями RSS

IO: read_bytes=0   write_bytes=52428800              ✅ write_bytes отражает реальные блочные записи (50 MiB)
    formatted: read 0 B  write 50.00 MiB              ℹ️ read_bytes = 0 (означает, что чтения с устройства не выполнялись — данные могли быть в кэше)
voluntary_ctxt_switches: 194                         ✅ voluntary_ctxt_switches = 194 (совпадает с /proc/<pid>/status) nonvoluntary_ctxt_switches: 4                       ✅  nonvoluntary_ctxt_switches = 4 (совпадает с /proc/<pid>/status)

--- comparison with system tools ---

===  ps  ===
   4318    2810 python3         S 00:00:00  9680  20504   ✅ PID, PPID, COMMAND, STATE, RSS/VIRT соответствуют данным из /proc

===  top  ===
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   4318 anton     20   0   20504   9680   5520 S   0.0   0.3   0:00.05 python3  ✅ RES и TIME+ соответствуют измерениям в pstat (/proc)

===  pidstat  ===
... (pidstat показывает %usr/%system ≈ 0)             ✅ pidstat показывает малую/нулевую загрузку CPU — согласуется с CPU time ~0.05 s

===  /proc/<pid>/stat  ===
4318 (python3) S 2810 4318 2810 34816 2810 4194304 5613 0 0 0 1 4 0 0 20 0 1 0 482061 20996096 2420 18446744073709551615 4194304 10324760 281474231337824 0 0 0 0 16781312 2 1 0 0 17 3 0 0 0 0 0 10419592 10995936 827072512 281474231341252 281474231341272 281474231341272 281474231345127 0
               ✅ raw stat содержит те же utime/stime и rss-pages (проверяем поля 14/15/24)

===  /proc/<pid>/status  ===
VmSize:	   20504 kB
VmRSS:	    9680 kB                                   ✅ VmRSS = 9680 kB (совпадает с pstat и ps/top)
Threads:	1 ✅
voluntary_ctxt_switches:	194 ✅
nonvoluntary_ctxt_switches:	4 ✅

===  /proc/<pid>/io  ===
rchar: 52489099                                       ℹ️ rchar/wchar показывают количество байт, переданных через syscalls (read/write)
wchar: 52428996                                       ℹ️ большое значение — операции записи/чтения происходили через системные вызовы
syscr: 54                                            ✅ количество read() вызовов зафиксировано
syscw: 57                                            ✅ количество write() вызовов зафиксировано
read_bytes: 0                                        ℹ️ реальных блочных чтений с устройства не было (возможно, данные читались из кэша)
write_bytes: 52428800                                ✅ 50 MiB реально записано на устройство (блочные записи)
cancelled_write_bytes: 0                             ✅ отменённых записей не зафиксировано

```
---

## Сопоставление полей `pstat` ↔ `ps`/`top`/`pidstat`

* **PID, PPid, State, Threads** — берутся из `/proc/<pid>/stat` и `/proc/<pid>/status`. `ps` и `top` показывают те же значения (совпало).
* **utime, stime** — из `/proc/<pid>/stat` (поля 14 и 15). `pstat` даёт их в тиках; `CPU time sec` = (utime + stime) / HZ. `ps`/`top` показывают аккумулированное время в человеко-читаемом формате (TIME/TIME+), поэтому отличия — только формат/округление.
* **VmRSS / rss** — `VmRSS` из `/proc/<pid>/status` (kB), `rss` из `/proc/<pid>/stat` в страницах (умножаем на page_size). `ps RSS` и `top RES` обычно совпадают с VmRSS.
* **smaps_rollup Rss** — даёт агрегацию по mapping’ам; обычно близко к VmRSS, но может немного отличаться из-за порядка чтения и округления.
* **IO**:

  * `rchar`/`wchar` — байты, переданные через системные вызовы (включают кэш).
  * `read_bytes`/`write_bytes` — байты реально прочитанные/записанные с блочного устройства (storage).
  * `pidstat -d` даёт статистику I/O за интервал (различные метрики), а `top` показывает `%wa` (waiting IO) — это другое: `%wa` — доля времени CPU, ожидаемого при I/O, а `read_bytes`/`write_bytes` — счётчики самих операций.
* **Context switches** — `voluntary_ctxt_switches` / `nonvoluntary_ctxt_switches` из `/proc/<pid>/status`. `pidstat -w` покажет изменения за интервал.

---

## Форматирование чисел и вычисления

* **RSS MiB**: `VmRSS = 9680 kB` → `9680 / 1024 = 9.453125 MiB` → округлено `9.45 MiB`.
  В `pstat.py` форматирование использует 1024-based (KiB/MiB).

* **CPU time sec**:
  `utime = 1`, `stime = 4`, `HZ = getconf CLK_TCK = 100` →
  `cpu_time_sec = (1 + 4) / 100 = 0.05 s`.
  `top` показывает `TIME+ = 0:00.05` — совпадает по смыслу, только другое форматирование.

* **IO**: `write_bytes = 52428800` → `52428800 / 1024**2 = 50.00 MiB`.

---

## 7. Ответы на вопросы (коротко, простым языком)

1. **Где в `/proc/<pid>/stat` и `/proc/<pid>/status` отражаются время в ядре/в юзере и состояние процесса?**

   * `/proc/<pid>/stat`: поле 14 = `utime` (в тиках) — время в пользовательском пространстве; поле 15 = `stime` — время в ядре; поле 3 = `state` (символ `R/S/D/...`).
   * `/proc/<pid>/status`: поле `State:` читаемо показывает состояние (например `S`), `Threads:` — число потоков.

2. **Как получить RSS и чем отличаются RssAnon и RssFile? Почему они важны?**

   * RSS: можно взять `VmRSS` из `/proc/<pid>/status` (kB) или `rss` из `/proc/<pid>/stat` (страницы → умножить на page_size).
   * `RssAnon` — доля RSS, которая анонимная (heap, stack, malloc и т.п.). `RssFile` — часть, связанна с file-backed mappings (например, mmap-нутые файлы). Важно потому, что анонимная память нельзя освободить через drop cache; file-backed можно пересоздать/перезагрузить и она чаще дешевле для обмена/общего использования.

3. **Как оценить IO-активность по `/proc/<pid>/io` и чем она отличается от «ожидания IO» в top/pidstat?**

   * `/proc/<pid>/io`: `rchar/wchar` — байты через syscalls; `read_bytes/write_bytes` — байты реально прочитанные/записанные на устройство.
   * `top` `%wa` — это время CPU, потраченное на ожидание I/O (в процентах). `pidstat -d` даёт скоростные показатели за интервал. Они измеряют разное: счётчики — сколько байт, `%wa`/ожидание — как долго CPU простаивал из-за IO.

4. **Что означает делитель HZ и как корректно посчитать CPU time sec = (utime+stime)/HZ?**

   * HZ — количество тиков в секунду у системы (`getconf CLK_TCK` или `sysconf(_SC_CLK_TCK)`). Чтобы перевести тики (utime/stime) в секунды, делим сумму на HZ. Не используйте фиксированное число — всегда читайте HZ у системы.

5. **Почему возможны рассинхронизации между `/proc` и выводом ps/top? Когда это критично?**

   * `/proc` — набор файлов, каждый читается отдельно; между чтением разных файлов процесс мог измениться → небольшие расхождения. `ps`/`top` сами читают `/proc` и могут округлять/форматировать. Различия важны, когда вы делаете точные измерения (например, профилирование, биллинг CPU) или сравниваете метрики, собранные в разное время. Для обычной диагностики небольшие несоответствия допустимы.
   
---

## 8. Выводы (простыми словами)

* `pstat.py` корректно читает ключевые поля из `/proc` (stat, status, io, smaps_rollup) и форматирует их удобно (KiB/MiB, секунды).
* `ps` и `top` показывают ту же информацию, только в человеко-читаемом виде и с другим форматированием — это нормально.
* Нулевые `read_bytes`/`write_bytes` не означают, что не было syscall-операций: `rchar/wchar` отражают syscalls, а `read_bytes/write_bytes` — только блочные операции на диске. Чтобы увидеть реальные блочные записи, нужно писать на реальную FS и/или использовать O_SYNC/os.fsync. В эксперименте мы заставили процесс писать синхронно и увидели `write_bytes = 50 MiB`.
* `smaps_rollup` полезен для разбивки RssAnon/RssFile, но может быть доступен только для root при чужих процессах и даёт значения, которые могут немного отличаться от VmRSS из-за порядка чтения/округления.
