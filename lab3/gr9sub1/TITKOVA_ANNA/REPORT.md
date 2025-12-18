#Лабораторная работа 3

Зависимости
```bash
sudo apt update
sudo apt install linux-tools-$(uname -r) linux-tools-generic
```

##C) /proc и собственная утилита pstat (обязательно)

Чтение файлов /proc
```bash
// В коде pstat.go реализованы функции для чтения всех требуемых файлов:
func parseProcStat(pid int) (*ProcStat, error)    // /proc/<pid>/stat
func parseProcStatus(pid int) (*ProcStatus, error) // /proc/<pid>/status  
func parseProcIO(pid int) (*ProcIO, error)        // /proc/<pid>/io
func parseSmapsRollup(pid int) (*SmapsRollup, error) // /proc/<pid>/smaps_rollup
```
Вывод требуемых полей 
```bash
// В коде printSummary

     // Print process summary
	fmt.Println("--- process summary ---")
	fmt.Printf("PID/Name : %d / %s\n", stat.Pid, stat.Comm)
	fmt.Printf("PPid     : %d    Threads: %d    State: %s\n", stat.Ppid, stat.NumThreads, stat.State)
	fmt.Printf("CPU(ticks): utime=%d stime=%d   CPU seconds=%d/%d = %.3fs\n",
		stat.UTime, stat.STime, stat.UTime+stat.STime, clkTck, cpuTimeSec)

	// Memory information
	fmt.Println("\n--- memory ---")
	fmt.Printf("Memory RSS : %d kB (%s)\n", status.VmRSS/1024, formatBytes(status.VmRSS))
	fmt.Printf("Stat RSS   : %d bytes (%s)\n", rssBytes, formatBytes(rssBytes))

	if status.VmSize > 0 {
		perc := float64(status.VmRSS) / float64(status.VmSize) * 100
		fmt.Printf("VmSize     : %d kB (%s)   RSS/VmSize = %.1f%%\n",
			status.VmSize/1024, formatBytes(status.VmSize), perc)
	}

	// Smaps information
	if smaps != nil {
		fmt.Println("\n--- smaps (rollup) ---")
		fmt.Printf("  Rss: %d kB (%s)\n", smaps.Rss/1024, formatBytes(smaps.Rss))
		fmt.Printf("  Anonymous: %d kB (%s)\n", smaps.Anon/1024, formatBytes(smaps.Anon))
		fmt.Printf("  File: %d kB (%s)\n", smaps.File/1024, formatBytes(smaps.File))
		fmt.Printf("  Shared: %d kB (%s)\n", smaps.Shmem/1024, formatBytes(smaps.Shmem))
	}

	// IO information
	if io != nil {
		fmt.Println("\n--- IO ---")
		fmt.Printf("rchar/wchar : %d / %d  (syscall bytes)\n", io.RChar, io.WChar)
		fmt.Printf("read_bytes/write_bytes: %d / %d  (%s / %s)\n",
			io.ReadBytes, io.WriteBytes, formatBytes(io.ReadBytes), formatBytes(io.WriteBytes))
	}   
```
Форматирование чисел
```bash
func formatBytes(bytes uint64) string {
	const (
		KB = 1024
		MB = KB * 1024
		GB = MB * 1024
	)

	switch {
	case bytes >= GB:
		return fmt.Sprintf("%.2f GiB", float64(bytes)/float64(GB))
	case bytes >= MB:
		return fmt.Sprintf("%.2f MiB", float64(bytes)/float64(MB))
	case bytes >= KB:
		return fmt.Sprintf("%.1f KiB", float64(bytes)/float64(KB))
	default:
		return fmt.Sprintf("%d B", bytes)
	}
}
```
Вычисления RSS MiB и CPU time
```bash
// Вычисление RSS в MiB
rssBytes := uint64(stat.RSSPages) * pageSize
formatBytes(rssBytes)  # → "6.88 MiB"

// Вычисление CPU time seconds = (utime+stime)/HZ  
clkTck := 100 // HZ value
cpuTimeSec := float64(stat.UTime+stat.STime) / float64(clkTck)  # → 0.090s
```
Сравнение с системными утилитами
```bash
func runSystemComparisons(pid int) {
	commands := []struct {
		name string
		cmd  string
	}{
		{"ps", fmt.Sprintf("ps -p %d -o pid,ppid,comm,state,time,rss,vsz --no-headers", pid)},
		{"top", fmt.Sprintf("top -b -n1 -p %d 2>/dev/null | head -20", pid)},
		{"pidstat", fmt.Sprintf("pidstat -p %d 1 1 2>/dev/null || echo '(pidstat not available)'", pid)},
	}

	for _, cmd := range commands {
		fmt.Printf("\n=== %s ===\n", cmd.name)
		output, err := runCommand(cmd.cmd)
		if err != nil {
			fmt.Printf("Error: %v\n", err)
		} else {
			fmt.Print(output)
		}
	}
}
```
Корректность вычислений - в секциях вывода ./run-task-c.sh 

Обработка особых случаев
```bash
smaps, err := parseSmapsRollup(pid)
if err != nil {
    fmt.Printf("Note: smaps_rollup not available: %v\n", err)
}
```

Запустите ./run-task-c.sh для демонстрации работы программы.

Выводы:
Метрики процесса из /proc успешно извлекаются и интерпретируются, были сопоставлены показатели /proc с утилитами ps, top, pidstat.
Все вычисления в pstat корректны и верифицированы.
Замечание: в выводе скрипта pidstat показывает 0% использования CPU, этосвязано ч тем, что замер выполнен во время фазы `sleep 30s`, когда процесс находится в состоянии `S (sleeping)` и не потребляет CPU ресурсы. Накопленное CPU время при этом равно 0.090s в pstat, что означает, что он работал ранее. Нулевые значения pidstat корректны и отражают текущее состояние процесса, а не ошибку измерений.


##E*) Диагностика и профилирование (со звёздочкой)

Cистемные вызовы 
```bash
# В run-task-e.sh строка 32:
timeout 6 strace -c -f ./bin/imitation-active 2>&1 | tee strace_output.log | tail -20
```
Аппаратные счетчики 
```bash
# В run-task-e.sh строки 37-44:
timeout 5 perf stat -e cycles,instructions,branches,branch-misses,context-switches,page-faults ./bin/imitation-active 2>&1 | head -25

timeout 4 perf stat -e task-clock,cpu-clock,context-switches,page-faults,cpu-migrations ./bin/imitation-active 2>&1 | head -20
```
Активный процесс для тестирования
```bash
// В imitation-active.go - создание CPU и IO нагрузки
go func() {
    for i := 0; i < 30; i++ {
        _ = fibonacci(1000)  // CPU нагрузка
        time.Sleep(1 * time.Second)
    }
}()

go func() {
    for i := 0; i < 5; i++ {
        writeSync(path, 5) // IO нагрузка
        readBack(path)
        os.Remove(path)
        time.Sleep(5 * time.Second)
    }
}()
```
Дополнительные компоненты
```bash
# В run-task-e.sh
./bin/imitation-active > /dev/null 2>&1 &
MONITOR_PID=$!
./bin/pstat $MONITOR_PID
ps -p $MONITOR_PID -o pid,ppid,comm,state,pcpu,pmem,rss,vsz,psr
```


Запустите ./run-task-e.sh для демонстрации работы программы.


Вывод исполняемого файла
Вывод strace:
```bash
100.00    0.576677         253      2272        29 total
```
2272 системных вызова - значительная активность
29 ошибок - низкий уровень, стабильная работа
Анализ в "тяжёлом режиме" (процесс активен)

Доказательства активности:
```bash
CPU(ticks): utime=193 stime=5   CPU seconds=198/100 = 1.980s
State: S (но CPU: 100% | MEM: 0.1%)
Threads: 4

```
100% CPU utilization - интенсивная нагрузка
4 потока - многопоточная архитектура
1.98s CPU времени - значительная работа
Состояние S но с активной работой - характерно для Go


Выводы по системным вызовам:
```bash
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ------------------
 34.85    0.215786         300       719        29 futex
 34.12    0.211256         265       796           nanosleep
 24.06    0.148952       12412        12           epoll_pwait
  3.63    0.022472        1605        14           write
  1.01    0.006260         894         7           read
```
futex (35%) - интенсивная синхронизация горутин Go
nanosleep (34%) - паузы между итерациями CPU нагрузки
epoll_pwait (24%) - эффективный асинхронный I/O
write/read (5%) - файловые операции

Счетчики perf не отображены из-за ограничений виртуальной машины.

Ответы на вопросы

Где в /proc/<pid>/stat и /proc/<pid>/status отражаются время в ядре/в юзере и состояние процесса?
В /proc/<pid>/stat:

utime (поле 14) - время в пользовательском режиме (jiffies)
stime (поле 15) - время в режиме ядра (jiffies)
state (поле 3) - состояние процесса: R (running), S (sleeping), D (disk sleep), Z (zombie), T (stopped)

В /proc/<pid>/status:

VoluntaryCtxtSwitches - добровольные переключения
NonvoluntaryCtxtSwitches - принудительные переключения
State - текстовое описание состояния

Пример из кода pstat:
```bash

stat.UTime = atou(rest[11])   // field 14 - utime
stat.STime = atou(rest[12])   // field 15 - stime  
stat.State = rest[0]          // field 3 - state
```

Как получить RSS и чем отличаются RssAnon и RssFile? Почему они важны?

Получение RSS:

Из /proc/<pid>/stat (поле 24): RSS = RSS_pages * PAGE_SIZE
Из /proc/<pid>/status: VmRSS в kB
Из /proc/<pid>/smaps_rollup: Rss в kB

Различия:

RssAnon - анонимная память (heap, stack)
RssFile - файловая память (memory-mapped files)
RssShmem - разделяемая память

Важность:

RssAnon показывает "чистую" память процесса
RssFile показывает кэшированные файлы
Помогает диагностировать утечки памяти и оптимизировать использование

Пример из кода:
```bash
rssBytes := uint64(stat.RSSPages) * pageSize
fmt.Printf("Rss: %d kB | Anonymous: %d kB | File: %d kB", 
    smaps.Rss/1024, smaps.Anon/1024, smaps.File/1024)
```

Как оценить IO‑активность по /proc/<pid>/io и чем она отличается от «ожидания IO» в top/pidstat?

Метрики /proc/<pid>/io:

rchar/wchar - байты прочитанные/записанные через системные вызовы
read_bytes/write_bytes - фактические байты на устройстве хранения
syscr/syscw - количество системных вызовов read/write

Отличия от top/pidstat:

/proc/pid/io - кумулятивная статистика за всё время жизни процесса
top/pidstat - моментальные значения или статистика за интервал
"Ожидание IO" в top показывает процессы в состоянии D (uninterruptible sleep)

Пример из кода:
```bash

fmt.Printf("rchar/wchar: %d / %d (syscall bytes)\n", io.RChar, io.WChar)
fmt.Printf("read_bytes/write_bytes: %d / %d (actual disk bytes)\n", 
    io.ReadBytes, io.WriteBytes)
```
  
Что означает делитель HZ и как корректно посчитать CPU time sec = (utime+stime)/HZ?

HZ (Hertz) - частота системного таймера ядра Linux:
Обычно 100 Hz (100 раз в секунду)
1 jiffy = 1/HZ секунд
utime/stime измеряются в jiffies


Почему возможны рассинхронизации между /proc и выводом ps/top? Когда это критично?

Причины рассинхронизаций:

Временные задержки - /proc обновляется мгновенно, ps/top могут кэшировать
Разные методы сбора - ps использует /proc, top может использовать другие источники
Момент снимка - процессы между чтением разных файлов /proc могут изменить состояние

Критичные случаи:

При мониторинге быстро меняющихся процессов
В высоконагруженных системах
При диагностике race conditions
Когда процесс завершается во время мониторинга


(Для E*) Что показывает strace -c и как интерпретировать perf stat?

strace -c показывает:

% time - распределение времени по системным вызовам
seconds - общее время на каждый тип вызова
usecs/call - среднее время одного вызова
calls - количество вызовов каждого типа
errors - количество ошибок

Интерпретация perf stat:

cycles - общее количество тактов CPU
instructions - количество выполненных инструкций
IPC (Instructions Per Cycle) = instructions/cycles - эффективность CPU
branches - количество условных переходов
branch-misses - ошибки предсказания переходов
cache-misses - промахи кэша




Выводы:
Достигнута глубокая наблюдаемость процессов, создана эффективная тестовая среда, доказана эффективность инструментов диагностики.
