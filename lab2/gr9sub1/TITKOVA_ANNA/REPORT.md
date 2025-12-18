###Задание A) Мини‑супервизор с воркерами

##Реализуйте процесс‑родителя (супервизор), который:
Порождает N воркеров (N ≥ 2)
```bash
// supervisor.go - функция Start()
func (s *Supervisor) Start() error {
    log.Printf("Starting %d workers", s.config.Workers)
    for i := 0; i < s.config.Workers; i++ {
        s.startWorker(i)
    }
    return s.mainLoop()
}
```

##Отслеживает их состояние через SIGCHLD, корректно «подбирает» зомби.
```bash
// supervisor.go - функция reapWorkers()
func (s *Supervisor) reapWorkers() {
    s.mutex.Lock()
    defer s.mutex.Unlock()
    
    for {
        var status syscall.WaitStatus
        pid, err := syscall.Wait4(-1, &status, syscall.WNOHANG, nil)
        if err != nil || pid <= 0 {
            break // Нет больше завершенных процессов
        }
        
        // Находим и обрабатываем завершенного воркера
        for id, cmd := range s.workers {
            if cmd.Process != nil && cmd.Process.Pid == pid {
                log.Printf("Worker %d (PID %d) exited with status %d", 
                    id, pid, status.ExitStatus())
                delete(s.workers, id)
                if !s.terminate {
                    log.Printf("Restarting worker %d", id)
                    go s.startWorker(id)
                }
                break
            }
        }
    }
}
```

##Обработка SIGTERM/SIGINT - graceful shutdown ≤ 5 секунд
```bash
// supervisor.go - функция gracefulShutdown()
func (s *Supervisor) gracefulShutdown() {
    log.Println("Initiating graceful shutdown...")
    s.broadcastSignal(syscall.SIGTERM)
    
    timeout := time.After(5 * time.Second)
    ticker := time.NewTicker(100 * time.Millisecond)
    
    for range ticker.C {
        allStopped := true
        s.mutex.Lock()
        for id, cmd := range s.workers {
            if cmd.Process != nil {
                if err := cmd.Process.Signal(syscall.Signal(0)); err == nil {
                    allStopped = false
                } else {
                    delete(s.workers, id)
                }
            }
        }
        s.mutex.Unlock()
        
        if allStopped {
            log.Println("All workers exited gracefully")
            break
        }
        
        select {
        case <-timeout:
            log.Println("Timeout reached - forcing shutdown")
            s.broadcastSignal(syscall.SIGKILL)
            return
        default:
        }
    }
}
```
##Обработка SIGHUP - graceful reload
```bash
// supervisor.go - функция reloadConfig()
func (s *Supervisor) reloadConfig() {
    log.Println("Reloading configuration...")
    config, err := loadConfig("config.json")
    if err != nil {
        log.Printf("Failed to reload config: %v", err)
        return
    }
    
    s.mutex.Lock()
    oldWorkers := s.config.Workers
    s.config = config
    s.mutex.Unlock()
    
    // Перезапускаем всех воркеров с новой конфигурацией
    for i := 0; i < config.Workers; i++ {
        if cmd, exists := s.workers[i]; exists && cmd.Process != nil {
            cmd.Process.Signal(syscall.SIGTERM)
            time.Sleep(50 * time.Millisecond)
        }
        s.startWorker(i)
    }
    
    // Останавливаем лишних воркеров если количество уменьшилось
    if oldWorkers > config.Workers {
        for i := config.Workers; i < oldWorkers; i++ {
            if cmd, exists := s.workers[i]; exists && cmd.Process != nil {
                cmd.Process.Signal(syscall.SIGTERM)
            }
            delete(s.workers, i)
        }
    }
}
```
##Обработка SIGUSR1/SIGUSR2 - переключение режимов

```bash
// supervisor.go - обработка в mainLoop()
if s.switchMode != "" {
    var sig syscall.Signal
    if s.switchMode == "light" {
        sig = syscall.SIGUSR1
    } else {
        sig = syscall.SIGUSR2
    }
    log.Printf("Broadcasting %s mode to all workers", s.switchMode)
    s.broadcastSignal(sig)
    s.switchMode = ""
}

// worker/main.go - обработчики сигналов
func handleSigusr1(sig os.Signal, config *Config) {
    mode = "light"
    log.Printf("[worker %d] Switched to LIGHT mode (%v)", os.Getpid(), sig)
}

func handleSigusr2(sig os.Signal, config *Config) {
    mode = "heavy" 
    log.Printf("[worker %d] Switched to HEAVY mode (%v)", os.Getpid(), sig)
}
```

##Авторестарт воркеров с ограничением частоты
```bash
// supervisor.go - функция startWorker()
func (s *Supervisor) startWorker(id int) {
    s.mutex.Lock()
    defer s.mutex.Unlock()
    
    // Проверка ограничения частоты рестартов
    now := time.Now()
    times := s.restartTimes[id]
    
    // Удаляем старые записи (старше 30 секунд)
    var recent []time.Time
    for _, t := range times {
        if now.Sub(t) <= 30*time.Second {
            recent = append(recent, t)
        }
    }
    
    // Проверяем лимит (не более 5 рестартов за 30 секунд)
    if len(recent) >= 5 {
        log.Printf("Worker %d restart rate limit exceeded (%d restarts in 30s)", 
            id, len(recent))
        return
    }
    
    // Запуск воркера...
    s.restartTimes[id] = append(recent, now)
}
```

##Воркеры выполняют имитацию работы

```bash
// worker/main.go - главный рабочий цикл
func main() {
    for !stopFlag {
        var workUS, sleepUS int
        
        if mode == "heavy" {
            workUS = config.ModeHeavy.WorkUS
            sleepUS = config.ModeHeavy.SleepUS
        } else {
            workUS = config.ModeLight.WorkUS  
            sleepUS = config.ModeLight.SleepUS
        }
        
        busyWaitMicros(workUS)
        statsTick++
        
        // Логирование каждую 10-ю итерацию
        if statsTick%10 == 0 {
            log.Printf("[worker %s] tick=%d mode=%s work_us=%d sleep_us=%d cpu=%d",
                workerID, statsTick, mode, workUS, sleepUS, cpuNum)
        }
        
        time.Sleep(time.Duration(sleepUS) * time.Microsecond)
    }
}
```

##Воркеры обрабатывают SIGTERM и SIGUSR1/2
```bash
// worker/main.go - обработчики сигналов
go func() {
    for sig := range sigCh {
        switch sig {
        case syscall.SIGTERM:
            handleSigterm(sig)
        case syscall.SIGUSR1:
            handleSigusr1(sig, config)
        case syscall.SIGUSR2:
            handleSigusr2(sig, config)
        }
    }
}()
```

##Формат конфигурации
```bash
// config_a.json
{
  "workers": 4,
  "mode_default": "heavy",
  "mode_heavy": {
    "work_us": 9000,
    "sleep_us": 1000
  },
  "mode_light": {
    "work_us": 2000,
    "sleep_us": 8000
  }
}
```

##Демонстрационные скрипты

demo_task_a_setup.sh - запуск системы
demo_task_a_monitor.sh - мониторинг процессов
demo_task_a_control.sh - управление сигналами

##Порядок запуска
Запустить make build-a в той директории, где Makefile.
Нужно 3 терминала, из той же директории в первом терминале запускаете chmod +x ./scripts/taskA/*.sh, затем ./scripts/taskA/setup.sh.
Во втором терминале - ./scripts/taskA/monitor.sh, в третьем - ./scripts/taskA/control.sh и наблюдайте за выполнением команд во 2-ом терминале.

##Результаты:
Супервизор успешно запускает 4 воркера (N ≥ 2), каждому воркеру присваивается уникальный PID, процессы с одинаковым проритетом. Изменение нагрузки изменяет показатели загруженности CPU, что видно в терминале 2 при выполнении скрипта, а также корректно обрабатывает сигналы, перезапускает воркеров и корректно завершается работа supervisor-а. 

##Выводы:
Планировщик Linux эффективно распределяет нагрузку между ядрами и процессами, учитывая, что процессы имеют равный приоритет.


###Задание B) Планирование: nice и CPU‑аффинити

##Установка разного nice для поднабора воркеров
```bash
# scripts/taskB/run_nice_demo.sh
./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "../$LOG_DIR/nice0.log" 2>&1 &
PID1=$!
echo "Процесс 1: PID $PID1, nice=0 (высокий приоритет)"

nice -n 10 ./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "../$LOG_DIR/nice10.log" 2>&1 &
PID2=$!
echo "Процесс 2: PID $PID2, nice=10 (низкий приоритет)"
```
##Сравнение распределения CPU
```bash
# scripts/taskB/run_nice_demo.sh
echo "=== СБОР МЕТРИК С PIDSTAT ==="
echo "Собираем метрики в течение 15 секунд..."
pidstat -p $PID1,$PID2 1 15 > "../$LOG_DIR/pidstat_nice.txt" 2>&1 &

echo ""
echo "=== МОНИТОРИНГ В РЕАЛЬНОМ ВРЕМЕНИ ==="
echo "PID     NI CPU %CPU COMMAND"
for i in {1..15}; do
    ps -p $PID1,$PID2 -o pid,ni,psr,pcpu,comm 2>/dev/null | tail -2
    echo "--- Снимок $i/15 ---"
    sleep 1
done
```
##Установка CPU-аффинити через taskset
```bash
# scripts/taskB/run_affinity_demo.sh
./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "../$LOG_DIR/affinity_cpu0.log" 2>&1 &
PID1=$!
echo "Процесс 1: PID $PID1, CPU 0"

./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 1 > "../$LOG_DIR/affinity_cpu1.log" 2>&1 &
PID2=$!
echo "Процесс 2: PID $PID2, CPU 1"
```

##Снятие метрик pidstat -u 1 10
```bash
# scripts/taskB/run_nice_demo.sh
pidstat -p $PID1,$PID2 1 15 > "../$LOG_DIR/pidstat_nice.txt" 2>&1 &

# scripts/taskB/run_combined_demo.sh  
pidstat -p $PID1,$PID2,$PID3,$PID4 1 20 > "../$LOG_DIR/pidstat_combined.txt" 2>&1 &
```
##Демонстрация влияния nice на %CPU

```bash
# scripts/taskB/run_nice_demo.sh - анализ результатов
echo "РАСПРЕДЕЛЕНИЕ CPU:"
CPU_NICE0=$(grep " $PID1 " "../$LOG_DIR/pidstat_nice.txt" 2>/dev/null | awk '{sum+=$8; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')
CPU_NICE10=$(grep " $PID2 " "../$LOG_DIR/pidstat_nice.txt" 2>/dev/null | awk '{sum+=$8; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')

echo "  Nice=0:  ${CPU_NICE0}% CPU"
echo "  Nice=10: ${CPU_NICE10}% CPU"
```
##Демонстрация влияния аффинити на распределение
```bash
# scripts/taskB/run_affinity_demo.sh - анализ распределения
echo "РАСПРЕДЕЛЕНИЕ ПО CPU ЯДРАМ:"
CORE0_COUNT=$(grep " $PID1 " "../$LOG_DIR/affinity_metrics.txt" 2>/dev/null | awk '{print $3}' | grep -c "^0$" || echo "0")
CORE1_COUNT=$(grep " $PID2 " "../$LOG_DIR/affinity_metrics.txt" 2>/dev/null | awk '{print $3}' | grep -c "^1$" || echo "0")
TOTAL_SNAPSHOTS=15

echo "  Процесс $PID1 (CPU 0): $CORE0_COUNT/$TOTAL_SNAPSHOTS снимков на ядре 0"
echo "  Процесс $PID2 (CPU 1): $CORE1_COUNT/$TOTAL_SNAPSHOTS снимков на ядре 1"
```

##Комбинированная демонстрация nice и аффинити
```bash
# scripts/taskB/run_combined_demo.sh
./cpu_burn --work-us 8000 --sleep-us 2000 --duration 25 --cpu 0 > "../$LOG_DIR/combo1.log" 2>&1 &
PID1=$!
echo "Процесс 1: CPU 0, nice=0"

nice -n 5 ./cpu_burn --work-us 8000 --sleep-us 2000 --duration 25 --cpu 0 > "../$LOG_DIR/combo2.log" 2>&1 &
PID2=$!
echo "Процесс 2: CPU 0, nice=5"

./cpu_burn --work-us 8000 --sleep-us 2000 --duration 25 --cpu 1 > "../$LOG_DIR/combo3.log" 2>&1 &
PID3=$!
echo "Процесс 3: CPU 1, nice=0"

nice -n 10 ./cpu_burn --work-us 8000 --sleep-us 2000 --duration 25 --cpu 1 > "../$LOG_DIR/combo4.log" 2>&1 &
PID4=$!
echo "Процесс 4: CPU 1, nice=10"
```
##Использование taskset для наглядности
```bash
# В скриптах используется внутренняя реализация через --cpu параметр
# что эквивалентно taskset -c 0 ./cpu_burn ...

# cfiles/cpu_burn.c - реализация set_affinity_optional()
#ifdef __linux__
#include <sched.h>
static void set_affinity_optional(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((unsigned)cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
    }
}
#endif
```
##Сохранение результатов для отчета

pidstat_nice.txt    - детальные метрики
nice0.log           - лог процесса nice=0
nice10.log          - лог процесса nice=10

affinity_metrics.txt - метрики распределения
affinity_cpu0.log    - лог процесса на CPU 0
affinity_cpu1.log    - лог процесса на CPU 1

##Запуск:
В терминале запускаете chmod +x ./scripts/taskB/*.sh, затем ./scripts/taskB/run_nice_demo.sh.
После выполнения можете запустить ./scripts/taskB/run_affinity_demo.sh.

##Результаты:
CPU affinity в логах affinity_metrics.txt закрепляет процессы на ядрах (пары PID и CPU не меняются) и равномерно распределяет нагрузку (%CPU примерно одинаковы), предотвращает миграцию и обеспечивает стабильную производительность.

Nice в логах pidstat_nice показывают, что nice значения влияют на распределение CPU (nice=0: 62.56%, nice=10: 31.18%) и процесс с большим приоритетом получает в 2 раза больше CPU. Время ожидания (%wait) отражает приоритет. Оба процесса на одном ядре и планировщик явно отдает предпочтение процессу с большим приоритетом. После начальной стабилизации распределение постоянное и планировщик последовательно применяет политику nice.

##Выводы:
CPU Affinity эффективен для:

Закрепления процессов на конкретных ядрах;
Предотвращения миграции между CPU;
Стабильной производительности без конкуренции.

Nice эффективен для:

Приоритизации задач внутри одного ядра;
Гарантии ресурсов для приоритетных процессов.


### D*) Память и OOM (со звёздочкой)

Постепенное увеличение RSS и мониторинг
```bash
# scripts/taskD/demo_memory_simple.sh
./mem_touch --rss-mb 128 --step-mb 16 --sleep-ms 300
```
Ограничение через ulimit/setrlimit
```bash
# scripts/taskD/demo_limits_simple.sh
./mem_touch --rss-mb 128 --step-mb 32 --sleep-ms 300 --limit-as-mb 80
```
Управление памятью через сигналы
```bash
# scripts/taskD/demo_signals_simple.sh
./mem_touch --rss-mb 96 --step-mb 16 --sleep-ms 400 &

# Управление в другом терминале:
kill -USR1 $MEM_PID  # Увеличить память (+16MB)
kill -USR2 $MEM_PID  # Уменьшить память (-16MB)
kill -TERM $MEM_PID  # Завершить процесс
```
Системный мониторинг памяти - запуск скрипта scripts/taskD/monitor_simple.sh

##Созданы 4 скрипта 
###Мониторинг
./scripts/taskD/monitor_simple.sh (в отдельном терминале для мониторинга запущенных процессов)

###Рост памяти
./scripts/taskD/demo_memory_simple.sh

Запускает mem_touch который постепенно увеличивает потребление памяти
Показывает рост VmSize и VmRSS в реальном времени
Демонстрирует как память выделяется шагами (16MB × 8 шагов = 128MB)

###Управление сигналами  
./scripts/taskD/demo_signals_simple.sh
Запускает mem_touch и показывает его PID
Дает команды для управления в другом терминале:

kill -USR1 PID - увеличить память (+16MB)
kill -USR2 PID - уменьшить память (-16MB)
kill -TERM PID - завершить процесс
###Ограничения памяти
./scripts/taskD/demo_limits_simple.sh

Тест 1: Без ограничений - процесс достигает 128MB
Тест 2: С RLIMIT_AS=80MB - процесс завершается при достижении лимита
Показывает разницу в поведении


Выводы:
Управление памятью - многоуровневый процесс, по уровням выглядит примерно так
Уровень приложения (malloc/free) -> Уровень процесса (setrlimit/RLIMIT_AS) -> Уровень ОС (OOM-killer, swap) -> Уровень мониторинга (/proc, pidstat).

RLIMIT_AS предотвращает исчерпание без вызова OOM-killer, который охватывает все адресное пространство и дает приложению шанс на graceful handling ошибок. Также памятью можно управлять с помощью сигналов и ограничений. 


##Ответы на вопросы

1. Чем процесс отличается от потока в Linux? Где это видно в ps и /proc?

Процесс в Linux — это независимая единица выполнения, которая имеет собственное виртуальное адресное пространство, дескрипторы файлов, сигналы и другие ресурсы. Он создаётся с помощью системного вызова fork() или clone() с флагами, определяющими новый процесс. Поток (thread) — это лёгковесный процесс (lightweight process, LWP), который делит адресное пространство, файлы и другие ресурсы с другими потоками в том же процессе, но имеет собственный стек, регистры и планирование. Потоки создаются с помощью clone() с флагом CLONE_VM (разделяемая память) или через библиотеку pthread.

В команде ps:

    По умолчанию ps показывает процессы, но потоки могут отображаться как отдельные записи с тем же PID (если использовать ps -eLf или ps -T для показа LWP).
    Опция -L показывает столбец LWP (ID потока) и NLWP (количество потоков в процессе).

В /proc:

    Каждый процесс имеет директорию /proc/<pid>/, где хранится информация о процессе (например, /proc/<pid>/status показывает VmSize, RSS и т.д.).
    Потоки видны как поддиректории в /proc/<pid>/task/<tid>/, где <tid> — ID потока. Файлы в этих поддиректориях аналогичны файлам процесса, но отражают состояние потока. Например, в /proc/<pid>/status поле Threads показывает количество потоков.

2. Как nice влияет на планирование CFS? Какие есть пределы/исключения?

Nice — это значение приоритета процесса (от -20 до 19), где более низкое (отрицательное) значение означает более высокий приоритет. В Completely Fair Scheduler (CFS), который является планировщиком по умолчанию в Linux с kernel 2.6.23, nice влияет на распределение CPU-времени через виртуальное время выполнения (vruntime). Процессы с более низким nice имеют больший вес (weight), рассчитываемый как weight = 1024 / (1.25 ^ nice), что приводит к большему доле CPU. CFS стремится к справедливости, корректируя vruntime так, чтобы процессы с высоким приоритетом получали больше времени.

Пределы:

    Диапазон: -20 (самый высокий приоритет) до 19 (самый низкий). Обычные пользователи могут устанавливать от 0 до 19, root — от -20 до 19.
    Исключения: Nice не влияет на процессы с политикой реального времени (SCHED_FIFO, SCHED_RR), где приоритет статический (1-99). Также в контейнерах (cgroups) nice может быть переопределён лимитами CPU shares. В многопроцессорных системах nice влияет глобально, но не гарантирует точное распределение из-за миграции задач.

3. Что даёт CPU‑аффинити и когда она вредна?

CPU affinity (привязка к CPU) позволяет закрепить процесс или поток за конкретными ядрами CPU с помощью системного вызова sched_setaffinity() или команды taskset. Это даёт:

    Улучшение производительности за счёт локальности кэша (cache affinity): данные остаются в кэше конкретного CPU, снижая задержки.
    Предотвращение миграции задач между CPU, что полезно для latency-sensitive приложений (например, в реальном времени, HPC или базах данных).
    Балансировку нагрузки вручную, например, выделение ядер для конкретных задач.

Когда вредна:

    Если нагрузка неравномерна: привязка к перегруженным ядрам приводит к простою других CPU и снижению общей производительности.
    В виртуализированных средах или с NUMA (Non-Uniform Memory Access): может нарушить доступ к памяти, увеличивая задержки.
    При динамических нагрузках: автоматический балансировщик kernel лучше справляется, а affinity может вызвать bottlenecks (например, если все задачи привязаны к одному CPU, а система имеет много ядер).
    В контейнерах: конфликтует с cgroup cpu.cpuset, приводя к неожиданному поведению.

4. Чем отличаются RLIMIT_AS, RLIMIT_DATA, RLIMIT_RSS? Почему RLIMIT_RSS часто игнорируется?

RLIMIT_* — это лимиты ресурсов, устанавливаемые с помощью setrlimit() для процессов (в shell — ulimit).

    RLIMIT_AS: Лимит на общее виртуальное адресное пространство процесса (включая stack, heap, mmap). Ограничивает malloc(), mmap() и другие аллокации. Полезен для предотвращения OOM от чрезмерного использования виртуальной памяти.
    RLIMIT_DATA: Лимит на размер сегмента данных (data segment), в основном heap (brk()/sbrk()). Не влияет на stack или mmap. Исторически использовался для старых систем, но в современных kernel менее релевантен, так как heap может расти через mmap.
    RLIMIT_RSS: Лимит на resident set size — объём физической памяти, занимаемой процессом (страницы в RAM). Если превышен, kernel может свопить страницы.

Почему RLIMIT_RSS часто игнорируется:

    Из-за сложности: точный контроль RSS требует постоянного мониторинга. Вместо этого kernel полагается на OOM killer и cgroups для управления памятью.

5. Почему возможны зомби и как их избежать при массовых рестартах воркеров?

Зомби-процессы возникают, когда дочерний процесс завершается (exit()), но родительский не вызывает wait() или waitpid() для получения статуса завершения. Kernel держит запись о зомби в таблице процессов (с флагом Z в ps), чтобы передать exit code родителю. Это занимает PID и ресурсы таблицы процессов, но не CPU/память. При массовых рестартах зомби накапливаются, если master-процесс не убирает детей timely, приводя к исчерпанию PID или замедлению.

Как избежать:

    В родительском процессе использовать сигнал SIGCHLD с обработчиком, который вызывает waitpid() в цикле (non-blocking, с WNOHANG).
    Для воркеров: В скриптах рестарта добавлять wait после kill, или использовать инструменты вроде supervisorctl, которые handle зомби.

6. Чем отличается «graceful shutdown» от «graceful reload/restart»? Какие последовательности безопасны?

Graceful shutdown: Это контролируемое завершение процесса, где он обрабатывает текущие запросы/соединения, закрывает их чисто (без потери данных) и выходит. Например, сервер ждёт завершения активных сессий, но не принимает новые. Обычно инициируется SIGTERM или SIGQUIT.

Graceful reload/restart: Это перезагрузка конфигурации или рестарт без прерывания сервиса. Процесс (master) загружает новую конфигурацию, спавнит новые worker'ы с ней, а старые завершают текущие задачи и выходят. Не останавливает сервис полностью. Обычно SIGHUP для reload, или комбинация для restart (например, в nginx: kill -USR2 для new master, затем -WINCH для old workers shutdown).

Безопасные последовательности:

    Для shutdown: Послать SIGTERM, подождать (timeout), если не завершилось — SIGKILL.
    Для reload: SIGHUP (nginx/apache) — master reloads config, workers перезапускаются корректно.
    Для restart: В nginx: kill -USR2 <pid> (new master), kill -WINCH <old_pid> (graceful shutdown old workers), kill -QUIT <old_pid> (если нужно). Или systemd: systemctl reload/restart.
    Общее: Всегда проверять PID-файл, избегать SIGKILL сразу, чтобы избежать повреждения данных.

7. Как повлияют контейнерные лимиты (cgroup v2) на наблюдаемые метрики процесса?

Cgroup v2 (unified hierarchy) лимитирует ресурсы (CPU, memory, IO, etc.) для групп процессов, влияя на метрики в /proc и инструментах вроде top/ps.

    CPU limits (cpu.max): Ограничивает CPU time (quota/period). В /proc/<pid>/stat: utime/stime уменьшаются, cputime throttled. В top: %CPU capped, может показывать высокий %CPU, но реальное использование ниже. Метрики в /sys/fs/cgroup/<group>/cpu.stat (usage, throttle events).
    Memory limits (memory.max): Ограничивает RAM+swap. В /proc/<pid>/status: VmSize/RSS capped, при exceed — OOM kill или reclaim. Метрики: memory.stat (usage, events like oom_kill). Процесс видит лимит как системный, но /proc/meminfo показывает host-значения (не cgroup).
    IO limits (io.max): Ограничивает IOPS/bps. В /proc/<pid>/io: read/write bytes throttled, delays в iostat.
    Общие: Процесс внутри контейнера видит метрики как "системные", но реальные ограничены (например, free не покажет cgroup limits). Используйте cgget или /sys/fs/cgroup для точных метрик. В Prometheus/node_exporter: cgroup-specific counters. Лимиты могут вызвать throttling, visible в kernel logs или perf.

