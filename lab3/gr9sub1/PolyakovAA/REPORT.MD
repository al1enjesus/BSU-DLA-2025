# Лабораторная 3 — /proc, собственные метрики и диагностика

Курс: Проектирование приложений под Linux (DLA, 4 курс)

## Цель работы

Изучить структуру и содержимое `/proc`, реализовать собственную утилиту `pstat <pid>` для чтения и интерпретации метрик процесса, сравнить её показатели с системными утилитами (`ps`, `top`, `pidstat`), а также освоить базовые приёмы диагностики приложений через `strace` и `perf`.

## Выполнение работы

### Используемая система и окружение
- ОС: **Ubuntu 24.04.3**  
- Ядро: **6.14.0-32-generic**  
- Кол-во CPU: **12**  
- Тестировался процесс с PID **2705**, принадлежащий `gnome-shell` (узнал через `htop`).  
  В качестве альтернативы можно было бы протестировать `$$` (текущий shell).  

### Реализация утилиты `pstat`

Код находится в `src/pstat.py`. Утилита:
- Читает `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup`
- Форматирует размеры в KiB/MiB
- Выводит PPid, Threads, State, utime/stime, ctxt_switches, VmRSS, RssAnon, RssFile, IO
- CPU time считается по формуле `(utime+stime)/HZ`

Особенность моей системы: в файле `/proc/<pid>/smaps_rollup` вместо `RssAnon`/`RssFile` используются названия **Anonymous** и **Pss_File**. Это было установлено командой:
cat /proc/2705/smaps_rollup | head -20

Соответственно, в коде я поправил ключи для корректного парсинга.

### Код и комментарии

```python
#!/usr/bin/env python3
import sys
import os

# HZ - количество тиков в секунду, обычно 100 в Linux
HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

def parse_kb_value(value_str):
    """Парсит строку вида '123 kB' в целое число килобайт"""
    if not value_str:
        return 0
    try:
        if " kB" in value_str:
            return int(value_str.replace(" kB", "").strip())
        else:
            return int(value_str.split()[0])
    except (ValueError, IndexError):
        return 0
        
def format_size(kb: int) -> str:
    """Форматирование KiB → MiB/KiB"""
    if kb >= 1024:
        return f"{kb/1024:.2f} MiB"
    return f"{kb} KiB"

def read_stat(pid):
    """Чтение /proc/pid/stat - основная статистика процесса"""
    with open(f"/proc/{pid}/stat") as f:
        fields = f.read().split()
    return fields

def read_status(pid):
    """Чтение /proc/pid/status - расширенная информация о процессе"""
    data = {}
    with open(f"/proc/{pid}/status") as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                data[k.strip()] = v.strip()
    return data

def read_io(pid):
    """Чтение /proc/pid/io - статистика ввода-вывода"""
    data = {}
    path = f"/proc/{pid}/io"
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                k, v = line.split(":", 1)
                data[k.strip()] = int(v.strip())
    return data

def read_smaps_rollup(pid):
    """Чтение /proc/pid/smaps_rollup - детальная информация о памяти"""
    data = {}
    path = f"/proc/{pid}/smaps_rollup"
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                if ":" in line:
                    k, v = line.split(":", 1)
                    # Важно: поля называются не RssAnon/RssFile, а Anonymous/Pss_File
                    data[k.strip()] = parse_kb_value(v.strip())
    return data

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <pid>")
        sys.exit(1)
    pid = sys.argv[1]

    # Чтение основных метрик
    stat = read_stat(pid)
    comm = stat[1].strip("()")  # имя процесса
    state = stat[2]             # состояние процесса
    ppid = stat[3]              # PID родительского процесса
    utime = int(stat[13])       # время в пользовательском режиме
    stime = int(stat[14])       # время в системном режиме
    cpu_time = (utime + stime) / HZ  # общее время CPU

    status = read_status(pid)
    threads = status.get("Threads", "?")                    # количество потоков
    voluntary_ctxt = status.get("voluntary_ctxt_switches", "?")  # добровольные переключения
    nonvoluntary_ctxt = status.get("nonvoluntary_ctxt_switches", "?")  # вынужденные переключения
    vmrss = status.get("VmRSS", "0 kB")                     # резидентная память

    # Чтение детальной информации о памяти
    smaps = read_smaps_rollup(pid)
    # ОСОБЕННОСТЬ: в данной системе поля называются Anonymous и Pss_File
    rss_anon = smaps.get("Anonymous", 0)  # анонимная память
    rss_file = smaps.get("Pss_File", 0)   # память файловых отображений

    # Статистика IO
    io = read_io(pid)
    read_bytes = io.get("read_bytes", 0)
    write_bytes = io.get("write_bytes", 0)

    # Вывод результатов
    print(f"PID: {pid} ({comm})")
    print(f"PPid: {ppid}")
    print(f"State: {state}")
    print(f"Threads: {threads}")
    print(f"CPU time: {cpu_time:.2f} sec")
    print(f"Voluntary ctxt switches: {voluntary_ctxt}")
    print(f"Nonvoluntary ctxt switches: {nonvoluntary_ctxt}")

    # Форматирование вывода памяти
    try:
        vmrss_kb = int(vmrss.split()[0])
    except:
        vmrss_kb = 0
    print(f"VmRSS: {format_size(vmrss_kb)}")
    print(f"  RssAnon: {format_size(rss_anon)}")
    print(f"  RssFile: {format_size(rss_file)}")

    # Вывод статистики IO
    print("IO:")
    print(f"  read_bytes: {format_size(read_bytes // 1024)}")
    print(f"  write_bytes: {format_size(write_bytes // 1024)}") 
```
    
#### Пример запуска:
./src/pstat.py 2705

### Сравнение результатов

#### pstat_result.txt
PID: 2705 (gnome-shell)
PPid: 2250
State: S
Threads: 20
CPU time: 294.44 sec
Voluntary ctxt switches: 2940719
Nonvoluntary ctxt switches: 4571
VmRSS: 319.29 MiB
RssAnon: 174.24 MiB
RssFile: 37.27 MiB
IO:
read_bytes: 84.38 MiB
write_bytes: 200 KiB

#### ps_result.txt

PID PPID S TIME THCNT RSS
2705 2250 S 00:04:54 20 326952


#### top_result.txt

PID USER PR NI VIRT RES SHR S %CPU %MEM TIME+
2705 andrei 20 0 4560968 326952 148676 S 0.0 2.0 4:54.45


#### pidstat.txt (усреднённые данные)

UID PID %usr %system %CPU Command
1000 2705 1.00 1.20 2.20 gnome-shell


#### perf_result.txt

118742231 cycles
146767664 instructions # 1.24 insn per cycle
28100171 branches
300447 branch-misses # 1.07%

> Примечание: perf для ядра 6.14.0-32 недоступен в регионе/репозиториях.  
> В `run1.sh` используется более ранняя версия perf (`6.8.0-85`), но она требует прав суперпользователя.  
> В `run2.sh` можно запускать без root при условии наличия актуальной версии perf.

#### strace_result.txt
Топ системных вызовов:

poll (58%)

restart_syscall (22%)

recvmsg (13%)

write/writev (≈4%)

прочие (read, ioctl, getpid, inotify_add_watch)


### Комментарии к результатам

- **pstat vs ps/top**: значения VmRSS совпадают (~326952 KiB = 319.29 MiB). Число потоков также совпадает. CPU time в `pstat` = 294.44 сек, что соответствует TIME в ps/top (≈ 4:54).
- **pidstat** показывает среднюю загрузку CPU в районе 2.2% для процесса `gnome-shell`. Это согласуется с тем, что в момент замеров процесс был в основном в состоянии `S` (sleeping).
- **IO** (`/proc/<pid>/io`) показывает, что процесс читал ≈84 MiB и писал всего 200 KiB. В `pidstat` и `top` это не отображается напрямую, там есть %wait (ожидание IO), которое у меня было равно 0.0%.
- **strace**: процесс в основном ждёт событий (`poll`), значительная часть — это `recvmsg` и системные вызовы, связанные с вводом-выводом. Это типично для графической оболочки (`gnome-shell`).
- **perf**: видим, что IPC ≈ 1.24 (инструкций на цикл), доля промахов по ветвлениям низкая (1.07%). Основная нагрузка — backend bound и bad speculation.

### Ответы на вопросы

1. **Где в /proc/<pid>/stat и /proc/<pid>/status отражаются время в ядре/в юзере и состояние процесса?**  
   - `/proc/<pid>/stat`: поля `utime` и `stime` — время в юзере/ядре; поле `state` — состояние.  
   - `/proc/<pid>/status`: поля `State` (состояние), `voluntary_ctxt_switches`, `nonvoluntary_ctxt_switches`.

2. **Как получить RSS и чем отличаются RssAnon и RssFile? Почему они важны?**  
   RSS = Resident Set Size, берётся из `/proc/<pid>/status` (VmRSS).  
   - RssAnon (`Anonymous`) — анонимные страницы (heap, stack).  
   - RssFile (`Pss_File`) — отображённые в память файлы.  
   Важно различать, т.к. рост RssAnon = утечки в памяти приложения, а рост RssFile = активное использование mmap-файлов.

3. **Как оценить IO‑активность по /proc/<pid>/io и чем она отличается от «ожидания IO» в top/pidstat?**  
   В `/proc/<pid>/io` фиксируются байты чтения/записи. В `top`/`pidstat` IO показывается как `%wait` (ожидание). Это разные метрики: первая — фактический объём, вторая — доля времени ожидания диска.

4. **Что означает делитель HZ и как корректно посчитать CPU time sec = (utime+stime)/HZ?**  
   HZ — частота тиков (обычно 100). CPU time вычисляется: `(utime+stime)/HZ`. У меня получилось ≈294.44 сек.

5. **Почему возможны рассинхронизации между /proc и выводом ps/top? Когда это критично?**  
   Метрики читаются в разные моменты времени. При быстроменяющихся процессах значения могут расходиться. Это критично при высоконагруженных сервисах, где важно точное время.

6. **(Для E*) Что показывает strace -c и как интерпретировать perf stat?**  
   - `strace -c` — топ системных вызовов по времени и числу обращений.  
   - `perf stat` — аппаратные счётчики: cycles, instructions, branches. Помогает оценить IPC, промахи ветвлений, узкие места (frontend/backend).

### Выводы

- Утилита `pstat` корректно парсит `/proc` и выдаёт аналогичные данные как `ps`, `top`, `pidstat`.  
- IO-активность процесса лучше видно через `/proc/<pid>/io`, тогда как в top/pidstat чаще фиксируется только ожидание.  
- Для `gnome-shell` характерно большое число `poll`-вызовов и низкая загрузка CPU.  
- `perf` и `strace` дают дополнительное понимание о характере работы процесса.  
- Ограничения: perf для ядра 6.14.0-32 недоступен, использовалась старая версия. Для запуска perf на новых ядрах может требоваться root.  
- В WSL2 perf и часть `/proc` могут быть недоступны, что стоит учитывать при повторении экспериментов.

## Артефакты

- `src/pstat.py` — исходник утилиты  
- `run1.sh` — запуск с perf (нужен root)  
- `run2.sh` — запуск с perf без root (если perf доступен)  
- `logs/ps_result.txt`, `logs/top_result.txt`, `logs/pidstat.txt`, `logs/pstat_result.txt`, `logs/perf_result.txt`, `logs/strace_result.txt` — результаты экспериментов

## Использование AI

- GPT-5 и Deepseek
- Составление отчета
- Prompt: Необходимо составить отчет в формате .md со всеми его фишками (вроде ``` ### и так далее) но выслать мне его текстом, чтобы я мог его скопировать, то есть он не должен для меня быть отформатирован а быть обычным текстом. Надо будет указать что RssAnon и RssFile у меня называется по другому и я это понял командой "cat /proc/2705/smaps_rollup | head -20". Указать что perf для версии 6.14.0-32 то ли недоступен из-за региона то ли еще не выпущен для данной версии ubuntu и поэтому в файле run1.sh он заменен на более раннюю версию 6.8.0-85 но требует прав суперпользователя, есть версия run2.sh где суперпользователь не нужен но с учетом того, что есть актуальная версия perf. Указать что был тестирован pid 2705 принадлежаший gnome-shell и я это узнал через htop, можно было и взять $$ текущего shell. Надо приложить свои комментарии КО ВСЕМ результатам и коду пайтон, то есть прокомментировать всё.
