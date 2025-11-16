# Лабораторная 3 — /proc, собственные метрики и диагностика

Кушмар Элина - 9 группа, кафедра ТП

Эта лаба продолжает тему процессов и наблюдаемости: читаем метрики процесса напрямую из `/proc`, сравниваем с системными утилитами и практикуем диагностику (`strace`, `perf`).

## Цели
- Уверенно извлекать и интерпретировать метрики процесса из `/proc`.
- Сопоставлять показатели `/proc` с утилитами `ps`, `top`, `pidstat`.
- Освоить базовую диагностику приложений: системные вызовы и аппаратные счётчики.

## Задания

### C) /proc и собственная утилита pstat
- Чтение `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup`:
1) 
```bash
cat /proc/5751/stat
```
вывод:
```text
5751 (gnome-terminal-) R 1875 5751 5751 0 -1 4194560 30916 0 0 0 563 86 0 0 20 0 7 0 29892 
660713472 14071 18446744073709551615 222217175629824 222217175944796 281474674516336 0 0 0 0 
4096 0 0 0 0 17 2 0 0 0 0 0 222217176011136 222217176027504 222218208251904 281474674518529 
281474674518564 281474674518564 281474674520021 0
```

2)
```bash
cat /proc/5751/status
```
вывод:
```text
Name:	gnome-terminal-
Umask:	0002
State:	S (sleeping)
Tgid:	5751
Ngid:	0
Pid:	5751
PPid:	1875
TracerPid:	0
Uid:	1000	1000	1000	1000
Gid:	1000	1000	1000	1000
FDSize:	256
Groups:	4 24 27 30 46 100 115 123 1000 
NStgid:	5751
NSpid:	5751
NSpgid:	5751
NSsid:	5751
Kthread:	0
VmPeak:	  645384 kB
VmSize:	  645228 kB
VmLck:	       0 kB
VmPin:	       0 kB
VmHWM:	   56420 kB
VmRSS:	   56284 kB
RssAnon:	   11260 kB
RssFile:	   41156 kB
RssShmem:	    3868 kB
VmData:	   59656 kB
VmStk:	     132 kB
VmExe:	     308 kB
VmLib:	   87672 kB
VmPTE:	     300 kB
VmSwap:	       0 kB
HugetlbPages:	       0 kB
CoreDumping:	0
THP_enabled:	1
untag_mask:	0xffffffffffffff
Threads:	7
SigQ:	0/14989
SigPnd:	0000000000000000
ShdPnd:	0000000000000000
SigBlk:	0000000000000000
SigIgn:	0000000000001000
SigCgt:	0000000100000000
CapInh:	0000000800000000
CapPrm:	0000000000000000
CapEff:	0000000000000000
CapBnd:	000001ffffffffff
CapAmb:	0000000000000000
NoNewPrivs:	0
Seccomp:	0
Seccomp_filters:	0
Speculation_Store_Bypass:	thread vulnerable
SpeculationIndirectBranch:	unknown
Cpus_allowed:	f
Cpus_allowed_list:	0-3
Mems_allowed:	00000000,00000001
Mems_allowed_list:	0
voluntary_ctxt_switches:	11564
nonvoluntary_ctxt_switches:	212
```

3)
```bash
cat /proc/5751/io
```
вывод:
```text
rchar: 4531232
wchar: 34379
syscr: 6707
syscw: 2353
read_bytes: 730112
write_bytes: 0
cancelled_write_bytes: 0
```

4) 
```bash
cat /proc/5751/smaps_rollup 2>/dev/null || echo "Файл smaps_rollup не найден"
```

```text
ca1af7d10000-ffffedfcf000 ---p 00000000 00:00 0                          [rollup]
Rss:               56544 kB
Pss:               21315 kB
Pss_Dirty:         13376 kB
Pss_Anon:          11444 kB
Pss_File:           7939 kB
Pss_Shmem:          1932 kB
Shared_Clean:      38956 kB
Shared_Dirty:       3568 kB
Private_Clean:      2328 kB
Private_Dirty:     11692 kB
Referenced:        55200 kB
Anonymous:         11444 kB
KSM:                   0 kB
LazyFree:              0 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
FilePmdMapped:         0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
Locked:                0 kB
```

Утилита `pstat <pid>`, которая:
- Читает `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup`
- Выводит краткую сводку: `PPid`, `Threads`, `State`, `utime/stime`, `voluntary_ctxt_switches`, `nonvoluntary_ctxt_switches`, `VmRSS`, `RssAnon`, `RssFile`, `read_bytes`, `write_bytes`.
- Умеет форматировать числа (МиБ/КиБ) и считает `RSS MiB` и «CPU time sec = (utime+stime)/HZ».

Вывод моей утилиты (файл pstat.py лежит в /src):
```text
PID: 5751
PPid: 1875
State: R
Threads: 7
utime: 505
stime: 75
CPU time sec: 5.80
VmRSS: 56164 kB
RSS MiB: 54.85
voluntary_ctxt_switches: 10210
nonvoluntary_ctxt_switches: 187
read_bytes: 730112
write_bytes: 0
```

## Сравнение показаний с ps, pidstat, top
### Сравнение с `ps`

```bash
ps -p 5751 -o pid,ppid,state,thcount,nlwp,utime,stime,rss,vsz,comm
```
**Вывод:**
```
  PID  PPID S THCNT NLWP  UTIME STIME   RSS    VSZ COMMAND
 5751  1875 S     7    7      - 15:06 56312 645228 gnome-terminal-
```

**Сравнение:**
- `PPid`: 1875 - совпадает
- `State`: S (sleeping) - теперь совпадает с /proc/5751/status, но отличается от R в pstat
- `Threads`: 7 - совпадает (THCNT и NLWP)
- `utime/stime`: в ps отображается как "-" для utime и "15:06" для stime, что отличается от числовых значений в pstat
- `VmRSS`: 56312 kB в ps vs 56164 kB в pstat - небольшое расхождение (148 kB)

### Сравнение с `pidstat`

```bash
pidstat -p 5751 1 1
```
**Вывод:**
```
Linux 6.14.0-15-generic (ubuntu) 	11/16/25 	_aarch64_	(4 CPU)

16:11:14      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
16:11:15     1000      5751    0.00    0.00    0.00    0.00    0.00     0  gnome-terminal-
Average:     1000      5751    0.00    0.00    0.00    0.00    0.00     -  gnome-terminal-
```

**Сравнение:**
- Процесс показывает 0% загрузки CPU, что соответствует его спящему состоянию

### Сравнение с `top`

```bash
top -p 5751 -n 1 -b
```
**Вывод:**
```
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   5751 ubuntu    20   0  645228  56440  45052 S   0.0   1.4   0:08.34 gnome-terminal-
```

**Сравнение:**
- `RES`: 56440 kB - близко к ps (56312 kB) и pstat (56164 kB)
- `S`: S (sleeping) - совпадает с ps и /proc/5751/status
- `TIME+`: 8.34 секунд vs 5.80 секунд в pstat - расхождение
- `VIRT`: 645228 kB - совпадает с VSZ в ps

### Анализ расхождений

1. **Состояние процесса (State)**:
  - pstat: R (running) - устаревшие данные
  - `ps/top/status`: S (sleeping) - актуальное состояние.
    Причина: pstat прочитала состояние из stat раньше, чем процесс перешел в sleep

2. **VmRSS**:
  - pstat: 56164 kB
  - ps: 56312 kB (разница 148 kB)
  - top: 56440 kB (разница 276 kB)  
Причина: постепенное изменение использования памяти процессом

3. **Время CPU**:
  - pstat: 5.80 секунд (utime+stime = 505+75 = 580 ticks / 100)
  - top: 8.34 секунд
  - ps: stime показывает 15:06 (15 минут 6 секунд?) - некорректное отображение  
    Причина: разные методы расчета и представления времени

4. **Архитектура**:
  - Система работает на aarch64 (ARM), что может влиять на некоторые метрики

### Выводы

1. Динамичность состояния: Основное расхождение в состоянии процесса (R vs S) демонстрирует, как быстро может меняться состояние процесса в Linux. pstat зафиксировала момент, когда процесс был в состоянии R, тогда как последующие проверки показывают S.
2. Метрики памяти: Расхождения в RSS (56164 vs 56312 vs 56440 kB) находятся в пределах 0.5%, что нормально для динамической системы.
3. Время CPU: Значительное расхождение во времени CPU (5.80s vs 8.34s) требует дополнительного исследования. Возможные причины:
  - Разный HZ на архитектуре aarch64
  - Накопление времени между замерами
  - Разные алгоритмы расчета в утилитах

Все основные метрики (PPid, Threads, порядок значений RSS) корректны, а наблюдаемые расхождения типичны для динамической системы Linux.


### E*) Диагностика и профилирование (со звёздочкой)
- Цель: Освоить инструменты диагностики `strace` и `perf` для анализа работы процесса.

- Шаги:
1. Запуск `strace -f -c` в легком и тяжелом режимах для gnome-shell (PID 2059)
2. Сбор аппаратных метрик через `perf stat`
3. Создание нагрузки: открытие Firefox, создание папок

```bash
sudo strace -f -c -p 2059 > strace_light.txt 2>&1 & STRACE_PID=$! echo "Strace запущен с PID: $STRACE_PID"
[1] 6370
Strace \u0437\u0430\u043f\u0443\u0449\u0435\u043d \u0441 PID: 6370
```
Результаты strace -c (легкий режим):
```text
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ------------------
 67.51    7.615513         842      9036       928 futex
 28.08    3.167997         867      3653           ppoll
  1.40    0.158233      158233         1         1 restart_syscall
  0.76    0.086135          23      3739           write
  0.53    0.060281          22      2681       218 read
  0.39    0.043936          20      2118      1597 recvmsg
  0.36    0.040829          57       707           ioctl
  0.18    0.020737          18      1132           mprotect
  0.18    0.020477          47       428           sendmsg
  0.14    0.016242          30       535           timerfd_settime
  0.13    0.014700          35       418           getpid
  0.09    0.009735         106        91           mmap
  0.07    0.007508          20       375           epoll_pwait
  0.05    0.005124          28       177           close
  0.03    0.003476          20       170           fcntl
  0.03    0.003384          48        70        70 inotify_add_watch
  0.02    0.001887          35        53           writev
  0.02    0.001703          22        75           munmap
  0.00    0.000549         274         2           clone3
  0.00    0.000468          26        18           prctl
  0.00    0.000372          37        10           madvise
  0.00    0.000338          42         8           rt_sigprocmask
  0.00    0.000187          23         8           getrusage
  0.00    0.000087          43         2           epoll_create1
  0.00    0.000087          43         2           set_robust_list
  0.00    0.000077          38         2           epoll_ctl
  0.00    0.000072          18         4         2 openat
  0.00    0.000066          16         4           fstat
  0.00    0.000061          20         3           timerfd_create
  0.00    0.000034          17         2           rseq
  0.00    0.000020          10         2           gettid
------ ----------- ----------- --------- --------- ------------------
100.00   11.280315         441     25526      2816 total
```
Результаты strace -c (тяжелые режим):
```text
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ------------------
 64.03   28.015434         659     42510      4837 futex
 33.42   14.622634         968     15094           ppoll
  0.75    0.329957          19     16819           write
  0.55    0.240975          20     11622       944 read
  0.41    0.179319          17     10388      8137 recvmsg
  0.22    0.098359          31      3134         6 ioctl
  0.11    0.047931          18      2636           timerfd_settime
  0.11    0.047589          24      1973           sendmsg
  0.09    0.040507          13      2934           mprotect
  0.08    0.033539          17      1871           getpid
  0.07    0.032355          16      1982           epoll_pwait
  0.04    0.016154          20       794           close
  0.04    0.015989          35       450           mmap
  0.02    0.009882          23       412           munmap
  0.02    0.009041          11       777           fcntl
  0.01    0.003268          33        98           writev
  0.01    0.002385        2385         1         1 restart_syscall
  0.01    0.002342          33        70        70 inotify_add_watch
  0.00    0.000955         119         8           setxattr
  0.00    0.000613          16        38           prctl
  0.00    0.000578          26        22         8 openat
  0.00    0.000445          24        18           mremap
  0.00    0.000301          75         4           rt_sigprocmask
  0.00    0.000276          19        14           epoll_ctl
  0.00    0.000244          20        12           getrusage
  0.00    0.000231           9        25           fstat
  0.00    0.000221          20        11           sendto
  0.00    0.000131         131         1           clone3
  0.00    0.000125         125         1           set_robust_list
  0.00    0.000111          27         4           accept4
  0.00    0.000087          43         2           brk
  0.00    0.000076          19         4           epoll_create1
  0.00    0.000070          70         1           socketpair
  0.00    0.000068          13         5           getsockopt
  0.00    0.000051           2        22           madvise
  0.00    0.000042          42         1           rseq
  0.00    0.000040          40         1           fallocate
  0.00    0.000018          18         1           gettid
  0.00    0.000016          16         1           memfd_create
  0.00    0.000000           0         3           timerfd_create
------ ----------- ----------- --------- --------- ------------------
100.00   43.752359         384    113764     14003 total
```

**Сравнение strace легкий vs тяжелый режим**:

| **Параметр**      | **Легкий режим**   | **Тяжелый режим** | **Изменение** | **Анализ**                                      |
|--------------------|--------------------|-------------------|---------------|------------------------------------------------|
| **Общее время**    | 11.28 сек         | 43.75 сек         | +387%         | Время мониторинга одинаковое, но активность выше |
| **Всего вызовов**  | 25,526            | 113,764           | +446%         | Значительный рост системной активности         |
| **futex**          | 67.5% (9,036)     | 64.0% (42,510)    | +470%         | Многопоточная синхронизация усилена            |
| **ppoll**          | 28.1% (3,653)     | 33.4% (15,094)    | +413%         | Увеличилась обработка событий UI               |
| **write**          | 0.8% (3,739)      | 0.8% (16,819)     | +450%         | Рост операций вывода                           |
| **read**           | 0.5% (2,681)      | 0.6% (11,622)     | +433%         | Рост операций ввода                            |
| **recvmsg**        | 0.4% (2,118)      | 0.4% (10,388)     | +490%         | Увеличилась IPC-коммуникация                   |

Вывод perf stat:
```bash
ubuntu@ubuntu:~/Desktop/lab3$ sudo perf stat -p 2059 sleep 5

 Performance counter stats for process id '2059':

            127.23 msec task-clock                       #    0.025 CPUs utilized             
               151      context-switches                 #    1.187 K/sec                     
                41      cpu-migrations                   #  322.260 /sec                      
              5801      page-faults                      #   45.596 K/sec                     
   <not supported>      cycles                                                                

       5.013614101 seconds time elapsed
```

```bash
ubuntu@ubuntu:~/Desktop/lab3$ sudo perf stat -e cycles,instructions,branches,branch-misses,cache-misses -p 2059 sleep 5

 Performance counter stats for process id '2059':

   <not supported>      cycles                                                                
   <not supported>      instructions                                                          
   <not supported>      branches                                                              
   <not supported>      branch-misses                                                         
   <not supported>      cache-misses                                                          

       5.003125438 seconds time elapsed
```

**Анализ perf stat**:  
- Низкая загрузка CPU: 0.025 CPUs utilized - процесс в основном ожидает событий
- Высокие page-faults: 45.596 K/sec - активная работа с памятью, типично для GUI
- Контекстные переключения: 1.187 K/sec - умеренная многопоточная активность
- Ограничение: Аппаратные счетчики не поддерживаются в текущем окружении

## Вопросы для отчёта
1. Где в `/proc/<pid>/stat` и `/proc/<pid>/status` отражаются время в ядре/в юзере и состояние процесса?   
В `/proc/<pid>/stat` время в юзере - 14-е поле (utime), в ядре - 15-е поле (stime). Состояние процесса - 3-е поле. В `/proc/<pid>/status` это в строках VmRSS, State.

2. Как получить `RSS` и чем отличаются `RssAnon` и `RssFile`? Почему они важны?
RSS (Resident Set Size) берется из VmRSS в status. RssAnon - память под данные процесса (куча, стек), RssFile - память под файлы (библиотеки). Важны чтобы понять, куда уходит память.

3. Как оценить IO‑активность по `/proc/<pid>/io` и чем она отличается от «ожидания IO» в `top/pidstat`? 
В `/proc/<pid>/io` смотрим read_bytes и write_bytes - сколько байт прочитано/записано. В `top/pidstat` "ожидание IO" - это время, когда процесс ждет дисковых операций, а не объем данных.

4. Что означает делитель `HZ` и как корректно посчитать `CPU time sec = (utime+stime)/HZ`?
HZ - это тики ядра в секунду, обычно 100. Чтобы посчитать CPU time sec, берем (utime + stime) из stat и делим на HZ. Получаем реальное время в секундах.

5. Почему возможны рассинхронизации между `/proc` и выводом `ps/top`? Когда это критично?
Рассинхронизации бывают потому что `/proc` - моментальный снимок, а ps/top могут читать с задержкой. Критично когда нужно точное состояние на конкретный момент.

6. (Для E*) Что показывает `strace -c` и как интерпретировать `perf stat`?
`strace -c` показывает какие системные вызовы делает процесс и сколько времени на них уходит. `perf stat` показывает низкоуровневые метрики типа тактов процессора, инструкций, кэш-промахов. Но у меня аппаратные счетчики не работали.
