# Вывод команды `python3 pstat.py <pid> --compare` до воркером информации
```
PID: 4318 [python3]
State: S   PPid: 2810   Threads: 1
utime (ticks): 1, stime (ticks): 0
CPU time (sec) = (utime+stime)/HZ = (1+0)/100 = 0.010 s

VmRSS (status): 9232 kB   (formatted: 9.02 MiB)
rss (stat * pagesize): 9453568 bytes (9.02 MiB)

smaps summary (kB):
  Rss_kB : 9360
  RssAnon_kB : 0
  RssFile_kB : None
  Total Rss from smaps_rollup: 9360 kB (9.14 MiB)

IO: read_bytes=0   write_bytes=0
    formatted: read 0 B  write 0 B

voluntary_ctxt_switches: 1
nonvoluntary_ctxt_switches: 1

--- comparison with system tools ---

===  ps  ===
   4318    2810 python3         S 00:00:00  9232  20476


===  top  ===
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   4318 anton     20   0   20476   9232   5264 S   0.0   0.3   0:00.01 python3


===  pidstat  ===
Linux 6.14.0-32-generic (anton-QEMU-Virtual-Machine) 	09/28/2025 	_aarch64_	(4 CPU)

06:50:01 PM   UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
06:50:02 PM  1000      4318    0.00    0.00    0.00    0.00    0.00     3  python3
Average:     1000      4318    0.00    0.00    0.00    0.00    0.00     -  python3


===  /proc/<pid>/stat  ===
4318 (python3) S 2810 4318 2810 34816 2810 4194304 1512 0 0 0 1 0 0 0 20 0 1 0 482061 20967424 2308 18446744073709551615 4194304 10324760 281474231337824 0 0 0 0 16781312 2 1 0 0 17 3 0 0 0 0 0 10419592 10995936 827072512 281474231341252 281474231341272 281474231341272 281474231345127 0


===  /proc/<pid>/status  ===
VmSize:	   20476 kB
VmRSS:	    9232 kB
Threads:	1
voluntary_ctxt_switches:	1
nonvoluntary_ctxt_switches:	1


===  /proc/<pid>/io  ===
rchar: 60299
wchar: 32
syscr: 39
syscw: 2
read_bytes: 0
write_bytes: 0
cancelled_write_bytes: 0
```

# Вывод команды `python3 pstat.py <pid> --compare` до воркером информации
```
PID: 4318 [python3]
State: S   PPid: 2810   Threads: 1
utime (ticks): 1, stime (ticks): 4
CPU time (sec) = (utime+stime)/HZ = (1+4)/100 = 0.050 s

VmRSS (status): 9680 kB   (formatted: 9.45 MiB)
rss (stat * pagesize): 9912320 bytes (9.45 MiB)

smaps summary (kB):
  Rss_kB : 9680
  RssAnon_kB : 0
  RssFile_kB : None
  Total Rss from smaps_rollup: 9680 kB (9.45 MiB)

IO: read_bytes=0   write_bytes=52428800
    formatted: read 0 B  write 50.00 MiB

voluntary_ctxt_switches: 194
nonvoluntary_ctxt_switches: 4

--- comparison with system tools ---

===  ps  ===
   4318    2810 python3         S 00:00:00  9680  20504


===  top  ===
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   4318 anton     20   0   20504   9680   5520 S   0.0   0.3   0:00.05 python3


===  pidstat  ===
Linux 6.14.0-32-generic (anton-QEMU-Virtual-Machine) 	09/28/2025 	_aarch64_	(4 CPU)

06:50:04 PM   UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
06:50:05 PM  1000      4318    0.00    0.00    0.00    0.00    0.00     3  python3
Average:     1000      4318    0.00    0.00    0.00    0.00    0.00     -  python3


===  /proc/<pid>/stat  ===
4318 (python3) S 2810 4318 2810 34816 2810 4194304 5613 0 0 0 1 4 0 0 20 0 1 0 482061 20996096 2420 18446744073709551615 4194304 10324760 281474231337824 0 0 0 0 16781312 2 1 0 0 17 3 0 0 0 0 0 10419592 10995936 827072512 281474231341252 281474231341272 281474231341272 281474231345127 0


===  /proc/<pid>/status  ===
VmSize:	   20504 kB
VmRSS:	    9680 kB
Threads:	1
voluntary_ctxt_switches:	194
nonvoluntary_ctxt_switches:	4


===  /proc/<pid>/io  ===
rchar: 52489099
wchar: 52428996
syscr: 54
syscw: 57
read_bytes: 0
write_bytes: 52428800
cancelled_write_bytes: 0
```
