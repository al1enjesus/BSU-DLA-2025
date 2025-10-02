## Что сделано

Написана утилита `pstat.py`, которая читает метрики процесса из `/proc/<pid>` и печатает читабельную сводку: PID, PPID, состояние, время в тиках, CPU-секунды, RSS, smaps (если есть), IO-счётчики и переключения контекста.
Есть тестовый скрипт `imitation.py`, который используется для генерации активности (sleep + IO).

---

## Как запустить

1. Запустить имитацию (в одном терминале):

```bash
chmod +x src/imitation.py
./src/imitation.py & echo $!
# либо
python3 src/imitation.py & echo $!
# запомни PID процесса
```

2. В отдельном терминале посмотреть сводку:

```bash
chmod +x src/pstat.py
python3 src/pstat.py <PID>
```

---

## Демонстрация работы

Ниже пример запуска `python3 src/pstat.py 4072`

```
pstat_personal vpstat-personal-1.0 — quick snapshot for PID 4072

system: CLK_TCK=100  PAGE_SIZE=4096 bytes
--- process summary ---
PID/Name : 4072 / python3
PPid     : 3862    Threads: 1    State: S
CPU(ticks): utime=2 stime=3   CPU seconds=5/100 = 0.050s

--- memory ---
Memory RSS : 9660 kB (9.43 MiB)
Stat RSS   : 9891840 bytes (9.43 MiB)
VmSize     : 20492 kB (20.01 MiB)   RSS/VmSize = 47.1%

--- smaps (rollup) ---
  Rss_kB: 9660
  RssAnon_kB: 0
  RssFile_kB: None
  Total Rss (smaps_rollup): 9660 kB (9.43 MiB)

--- IO ---
rchar/wchar : 26274501 / 26214575  (syscall bytes)
read_bytes/write_bytes: 0 / 26214400  (0 B / 25.00 MiB)

--- context switches ---
voluntary   : 99
non-voluntary: 1

--- comparison with system tools ---

=== ps ===
   4072    3862 python3         S 00:00:00  9660  20492


=== top ===
    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
   4072 anton     20   0   20492   9660   5516 S   0.0   0.3   0:00.05 python3


=== pidstat ===
Linux 6.14.0-32-generic (anton-QEMU-Virtual-Machine) 	10/02/2025 	_aarch64_	(4 CPU)

06:22:51 PM   UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
06:22:52 PM  1000      4072    0.00    0.00    0.00    0.00    0.00     1  python3
Average:     1000      4072    0.00    0.00    0.00    0.00    0.00     -  python3


=== /proc/<pid>/stat ===
4072 (python3) S 3862 4072 3862 34817 3862 4194304 5549 0 0 0 2 3 0 0 20 0 1 0 158920 20983808 2415 18446744073709551615 4194304 10324760 281474928076224 0 0 0 0 16781312 2 1 0 0 17 1 0 0 0 0 0 10419592 10995936 744890368 281474928079098 281474928079125 281474928079125 281474928082919 0


=== /proc/<pid>/status ===
VmSize:	   20492 kB
VmRSS:	    9660 kB
Threads:	1
voluntary_ctxt_switches:	99
nonvoluntary_ctxt_switches:	1


=== /proc/<pid>/io ===
rchar: 26274501
wchar: 26214575
syscr: 48
syscw: 31
read_bytes: 0
write_bytes: 26214400
cancelled_write_bytes: 0
```

---

## Сравнение с системными утилитами

Здесь собраны команды, которые удобно выполнить для проверки того, что показывает `pstat.py`.

**Команды для сравнения:**

```bash
ps -p <PID> -o pid,ppid,comm,state,time,rss,vsz
top -b -n1 -p <PID>
pidstat -p <PID> 1 1      
cat /proc/<PID>/stat
grep -E 'VmRSS|VmSize|Threads|voluntary_ctxt_switches|nonvoluntary_ctxt_switches' /proc/<PID>/status
cat /proc/<PID>/io
sudo cat /proc/<PID>/smaps_rollup   # если нужно и есть доступ
```

**Что сверять:**

* `ps`/`top`: сравниваем PID, PPID, состояние, RSS (RES в top). Эти числа обычно равны `VmRSS`/`rss`.
* `pidstat`: покажет загрузку CPU за интервал — полезно для динамики (в нашем случае CPU мало).
* `/proc/<PID>/io`: `rchar/wchar` — сколько байт через syscalls; `read_bytes/write_bytes` — сколько реально пошло на диск (блочные операции).
* `/proc/<PID>/stat`: там находятся `utime`/`stime` (поля 14/15) и `rss` в страницах (нужно умножить на page_size).

---

## Ответы на вопросы

1. **Где видно время в юзере и в ядре, и где состояние процесса?**
   В файле `/proc/<pid>/stat` есть много полей: поле 14 — `utime` (время в user), поле 15 — `stime` (время в kernel). То, что человек обычно видит в `top`/`ps` как `TIME`/`TIME+`, — это та же сумма, но уже в секундах и в удобном формате. Состояние процесса (S, R, D и т.п.) тоже указано в `stat` (поле 3) и в `status` в текстовой форме.

2. **Как взять RSS и что такое RssAnon и RssFile?**
   RSS (resident set) — это сколько памяти процесса реально занимает в ОЗУ. Можно читать `VmRSS` из `/proc/<pid>/status` (кБ) или вычислять `rss` из `/proc/<pid>/stat` (страницы × page_size).
   `RssAnon` — часть RSS, которая анонимная (heap/stack/calloc). `RssFile` — часть, которая связана с отображёнными файлами (mmap на файлы). Это важно, потому что file-backed страницы легче разделять/освобождать и они могут быть общими между процессами, а анонимная память — «личная».

3. **Как понять I/O по `/proc/<pid>/io` и почему это не одно и то же с `%wa` в top?**
   В `/proc/<pid>/io` есть `rchar/wchar` — сколько байт процесс прочитал/записал через системные вызовы, и `read_bytes/write_bytes` — сколько реально прошло через блочное устройство (дисковые операции). `%wa` в `top` — это процент времени CPU, потраченного на ожидание I/O. То есть `/proc/<pid>/io` — это счётчики объёма данных, а `%wa` — измерение времени ожидания.

4. **Что такое HZ и как переводить тики в секунды?**
   HZ — число тиков в секунду у системы (узнаётся командой `getconf CLK_TCK` или через sysconf). `utime` и `stime` даны в тиках, чтобы получить секунды, складываем их и делим на HZ: `(utime + stime) / HZ`.

5. **Почему выводы из `/proc` и `ps`/`top` иногда не совпадают?**
   Потому что данные читаются в разные моменты: `/proc` — это набор отдельных файлов, и между чтениями отдельных файлов состояние процесса может немного измениться. `ps`/`top` делают своё чтение и затем форматируют. Небольшие расхождения — нормальны; если нужна точность, нужно делать снэпшот всех полей одновременно или повторять замеры.

---

## Экспериментальные замечания

* В примере видно, что после работы воркера `write_bytes = 25 MiB`. Это означает: были реальные блочные записи на диск (или в файловую систему, не в tmpfs).
* При этом `read_bytes = 0`, но `rchar`/`wchar` большие — значит процесс делал read/write вызовы, но `read_bytes` не увеличился (чтение шло из кэша или просто не требовало блочных обращений).
* RSS вырос до ~9.43 MiB — это видно в `VmRSS` и в `smaps_rollup`. Небольшие несоответствия между этими числами возможны (время между чтениями, округления).

---

## Выводы

* `pstat.py` корректно читает основные метрики процесса и показывает их в удобном виде.
* `rchar/wchar` и `read_bytes/write_bytes` показывают разное: первые — syscalls, вторые — реальные диск-операции.
* Небольшие расхождения между источниками — нормальны; важно понимать, что именно измеряется.


