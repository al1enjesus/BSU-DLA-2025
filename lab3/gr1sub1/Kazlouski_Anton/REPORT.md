
# Отчёт по лабораторной 3 — /proc, собственные метрики и диагностика (macOS)


## Цель
Освоить извлечение метрик процесса без /proc, используя libproc/Mach, сравнить с ps/top/vmmap.

---

## Ход работы
1. Реализована утилита `pstat <pid>` на C.
2. Получены поля: PPid, Threads, State, utime, stime, VmRSS, VmSize, voluntary ctxt switches.
3. Сравнены значения с ps, top, vmmap.
4. Диагностика IO через fs_usage и системные вызовы через dtruss.
5. Результаты задокументированы.

---

## Фрагменты вывода

```bash
qooriq@MacBook-Pro-Anton-2 src % ./pstat 62913                                                
=== pstat summary for PID 62913 ===
Command: clion
PPid: 1
Threads: 125
State: 2 (see 'ps -o state -p 62913')
User time: 5341.711 s
System time: 869.164 s
Total CPU time: 6210.875 s
VmRSS: 1466.92 MiB
VmSize: 409496.94 MiB
Read bytes: n/a (use fs_usage or proc_pid_rusage)
Write bytes: n/a (use fs_usage or proc_pid_rusage)
Voluntary ctxt switches: 2544097
Involuntary ctxt switches: n/a
```

``` bash
ps -p 62913 -o pid,ppid,state,time,rss,vsz,comm

PID  PPID STAT      TIME    RSS      VSZ COMM
62913     1 S      4:02.91 1504912 419321952 /Applications/CLion.app/Contents/MacOS/clion

```

``` bash
qooriq@MacBook-Pro-Anton-2 src % top -pid 62913 -stats pid,command,cpu,time,rsize | head -n 10

Processes: 602 total, 3 running, 599 sleeping, 3022 threads
2025/10/06 23:50:08
Load Avg: 7.08, 7.47, 7.68
CPU usage: 18.75% user, 14.47% sys, 66.77% idle
SharedLibs: 510M resident, 88M data, 35M linkedit.
MemRegions: 255516 total, 4188M resident, 182M private, 1809M shared.
PhysMem: 15G used (2279M wired, 5365M compressor), 58M unused.
VM: 234T vsize, 4729M framework vsize, 5207999(0) swapins, 6574263(0) swapouts.
Networks: packets: 289586362/168G in, 212603979/46G out.
Disks: 271237468/3426G read, 44231211/908G written.
```

``` bash
qooriq@MacBook-Pro-Anton-2 src % vmmap -summary 62913

Process:         clion [62913]
Path:            /Applications/CLion.app/Contents/MacOS/clion
Load Address:    0x1021f8000
Identifier:      clion
Version:         ???
Code Type:       ARM64
Platform:        macOS
Parent Process:  ??? [1]

Date/Time:       2025-10-06 23:50:53.115 +0300
Launch Time:     2025-10-06 22:03:18.250 +0300
OS Version:      macOS 14.1.1 (23B81)
Report Version:  7
Analysis Tool:   /usr/bin/vmmap

Physical footprint:         1.7G
Physical footprint (peak):  1.8G
Idle exit:                  untracked
----

ReadOnly portion of Libraries: Total=1.9G resident=457.9M(24%) swapped_out_or_unallocated=1.4G(76%)
Writable regions: Total=3.8G written=142.4M(4%) resident=961.8M(25%) swapped_out=537.5M(14%) unallocated=2.3G(61%)

                                VIRTUAL RESIDENT    DIRTY  SWAPPED VOLATILE   NONVOL    EMPTY   REGION 
REGION TYPE                        SIZE     SIZE     SIZE     SIZE     SIZE     SIZE     SIZE    COUNT (non-coalesced)
===========                     ======= ========    =====  ======= ========   ======    =====  =======
Accelerate framework               128K       0K       0K     128K       0K       0K       0K        1
Activity Tracing                   256K      32K      32K       0K       0K      32K       0K        1
CG image                          1712K     544K     544K    1168K       0K       0K       0K       29
ColorSync                          608K      16K      16K     592K       0K       0K       0K       28
CoreAnimation                     1200K    1024K    1024K     160K       0K    1024K       0K       69
CoreGraphics                        32K      32K      32K       0K       0K       0K       0K        2
CoreUI image data                 2064K       0K       0K    2064K       0K       0K       0K       15
Foundation                          48K      16K      16K       0K       0K       0K      32K        2
IOAccelerator                    238.0M   161.4M   161.4M    29.3M    47.7M   113.5M    23.4M      921
IOKit                             1248K    1248K    1248K       0K       0K      16K       0K       78
IOSurface                         85.5M    85.5M    85.5M       0K       0K    85.5M       0K        3
Image IO                            16K       0K       0K       0K       0K       0K      16K        1
Kernel Alloc Once                   32K      16K      16K       0K       0K       0K       0K        1
MALLOC guard page                  192K       0K       0K       0K       0K       0K       0K       12
MALLOC metadata                    336K     304K     304K      32K       0K       0K       0K       15
MALLOC_LARGE                      7616K    1024K    1024K       0K    1024K       0K    6528K       28         see MALLOC ZONE table below
MALLOC_LARGE metadata               16K      16K      16K       0K       0K       0K       0K        1         see MALLOC ZONE table below
MALLOC_MEDIUM                      1.2G    46.8M    39.9M   263.0M       0K       0K       0K       10         see MALLOC ZONE table below
MALLOC_NANO                        1.0G    96.1M    96.1M    3632K       0K       0K       0K        2         see MALLOC ZONE table below
MALLOC_SMALL                     120.0M    57.8M    57.8M    27.6M       0K       0K       0K       15         see MALLOC ZONE table below
MALLOC_TINY                       34.0M    21.1M    21.1M     576K       0K       0K       0K       34         see MALLOC ZONE table below
MALLOC_TINY (empty)               1024K      32K      32K       0K       0K       0K       0K        1         see MALLOC ZONE table below
Mach message                       192K      96K      96K      96K       0K       0K       0K        9
STACK GUARD                       4416K       0K       0K       0K       0K       0K       0K      123
Stack                            354.5M    6576K    6576K    2848K       0K       0K       0K      123
Stack Guard                       57.2M       0K       0K       0K       0K       0K       0K       77
VM_ALLOCATE                        3.9G   729.9M   729.9M   238.3M       0K       0K       0K     5911
VM_ALLOCATE (reserved)            32.0M       0K       0K       0K       0K       0K       0K        2         reserved VM address space (unallocated)
__AUTH                            3280K    1683K     265K     195K       0K       0K       0K      552
__AUTH_CONST                      40.2M    19.0M       0K       0K       0K       0K       0K      809
__CTF                               824      824       0K       0K       0K       0K       0K        1
__DATA                            25.1M    7665K    2444K    1646K       0K       0K       0K      825
__DATA_CONST                      50.1M    20.3M    1824K    8464K       0K       0K       0K      844
__DATA_DIRTY                      2870K    1597K     532K      92K       0K       0K       0K      319
__FONT_DATA                          4K       4K       0K       0K       0K       0K       0K        1
__INFO_FILTER                         8        8       0K       0K       0K       0K       0K        1
__LINKEDIT                       895.2M    33.6M       0K       0K       0K       0K       0K       29
__OBJC_RO                         70.8M    33.0M       0K       0K       0K       0K       0K        1
__OBJC_RW                         2156K    1628K      60K      16K       0K       0K       0K        1
__TEXT                             1.0G   424.3M       0K       0K       0K       0K       0K      862
dyld private memory                272K      80K      80K       0K       0K       0K       0K        2
mapped file                        3.6G   312.6M      48K       0K       0K       0K       0K      496
shared memory                     3360K     768K     768K    1264K       0K       0K       0K       41
unused but dirty shlib __DATA      319K     203K     203K     115K       0K       0K       0K      244
===========                     ======= ========    =====  ======= ========   ======    =====  =======
TOTAL                             12.7G     2.0G     1.2G   580.7M    48.7M   200.0M    29.8M    12542
TOTAL, minus reserved VM space    12.7G     2.0G     1.2G   580.7M    48.7M   200.0M    29.8M    12542

                                          VIRTUAL   RESIDENT      DIRTY    SWAPPED ALLOCATION      BYTES DIRTY+SWAP          REGION
MALLOC ZONE                                  SIZE       SIZE       SIZE       SIZE      COUNT  ALLOCATED  FRAG SIZE  % FRAG   COUNT
===========                               =======  =========  =========  =========  =========  =========  =========  ======  ======
MallocHelperZone_0x102730000                 1.4G     125.1M     118.2M     291.2M      50973     345.1M      64.3M     16%      49
DefaultMallocZone_0x10276c000                1.0G      96.1M      96.1M      3632K    2389284      97.3M      2328K      3%       2
QuartzCore_0x17fff8000                      18.0M       592K       592K         0K       3054       225K       367K     62%      11
DefaultPurgeableMallocZone_0x379aa8000      7632K      1040K      1040K         0K         28      7616K         0K      0%      29
===========                               =======  =========  =========  =========  =========  =========  =========  ======  ======
TOTAL                                        2.4G     222.8M     215.9M     294.7M    2443339     450.1M      60.5M     12%      91
```



Вот мои выводы

### pstat

| Поле             | pstat             | ps/top/vmmap                   | Комментарий                                         |
|-----------------|-----------------|--------------------------------|----------------------------------------------------|
| PPid             | 1               | 1                              | Совпадает                                         |
| Threads          | 125             | 125 (pstat)                    | ps не показывает на macOS                          |
| State            | 2               | S                              | Код состояния отличается, совпадение по смыслу     |
| Total CPU time   | 6210.875 s      | 4:02.91 (242.91 s)             | Разница из-за моментального snapshot ps           |
| VmRSS            | 1466.92 MiB     | top RSIZE ≈ 2.0G, ps RSS 1.47G | Совпадает почти полностью                          |
| VmSize           | 409496.94 MiB   | ps VSZ 409496 MiB              | Совпадает                                         |
| RssAnon          | n/a             | vmmap 961.8M                    | Берётся из vmmap                                  |
| RssFile          | n/a             | vmmap 142.4M                    | Берётся из vmmap                                  |
| Voluntary ctxt switches | 2 544 097 | n/a                            | Только pstat                                      |
| IO bytes         | n/a             | fs_usage / dtruss              | Используется отдельный инструмент                 |

