# Вывод команды `python3 pstat.py <pid> --compare` до записи воркером информации
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

# Вывод команды `python3 pstat.py <pid> --compare` после записи воркером информации
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
