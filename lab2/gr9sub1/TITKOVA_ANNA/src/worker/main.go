package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

var (
	stopFlag  bool
	mode      string
	statsTick int
)

type Config struct {
	ModeDefault string     `json:"mode_default"`
	ModeHeavy   ModeConfig `json:"mode_heavy"`
	ModeLight   ModeConfig `json:"mode_light"`
}

type ModeConfig struct {
	WorkUS  int `json:"work_us"`
	SleepUS int `json:"sleep_us"`
}

func handleSigterm(sig os.Signal) {
	log.Printf("[worker %d] Received %v -> stopping after current cycle", os.Getpid(), sig)
	os.Exit(0)
}

func handleSigusr1(sig os.Signal, config *Config) {
	mode = "light"
	log.Printf("[worker %d] Switched to LIGHT mode (%v) - WORK_US: %d, SLEEP_US: %d",
		os.Getpid(), sig, config.ModeLight.WorkUS, config.ModeLight.SleepUS)
}

func handleSigusr2(sig os.Signal, config *Config) {
	mode = "heavy"
	log.Printf("[worker %d] Switched to HEAVY mode (%v) - WORK_US: %d, SLEEP_US: %d",
		os.Getpid(), sig, config.ModeHeavy.WorkUS, config.ModeHeavy.SleepUS)
}

func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read config: %w", err)
	}

	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		return nil, fmt.Errorf("failed to parse config JSON: %w", err)
	}

	return &config, nil
}

func busyWaitMicros(us int) {
	start := time.Now()
	end := start.Add(time.Duration(us) * time.Microsecond)

	// Busy loop - компилятор не оптимизирует из-за runtime.Gosched()
	for time.Now().Before(end) {
		runtime.Gosched()
	}
}

func trySetNice(nice int) {
	// Проверяем диапазон nice (от -20 до 19)
	if nice < -20 || nice > 19 {
		log.Printf("Nice value %d out of range (-20 to 19)", nice)
		return
	}

	err := syscall.Setpriority(syscall.PRIO_PROCESS, 0, nice)
	if err != nil {
		log.Printf("Cannot set nice=%d: %v", nice, err)
	} else {
		log.Printf("Set nice=%d", nice)
	}
}

func trySetAffinity(cpus string) {
	cpuList := strings.Split(cpus, ",")

	if len(cpuList) == 0 {
		log.Printf("No valid CPUs specified")
		return
	}

	// Создаем CPU set mask
	var cpuSet unix.CPUSet
	cpuSet.Zero()

	for _, cpuStr := range cpuList {
		cpu, err := strconv.Atoi(strings.TrimSpace(cpuStr))
		if err != nil {
			log.Printf("Invalid CPU value: %s", cpuStr)
			return
		}
		if cpu < 0 || cpu >= 1024 { // Максимум 1024 CPU в Linux
			log.Printf("CPU %d out of range (0-1023)", cpu)
			return
		}
		cpuSet.Set(cpu)
	}

	// Устанавливаем affinity
	err := unix.SchedSetaffinity(0, &cpuSet)
	if err != nil {
		log.Printf("Cannot set affinity=%s: %v", cpus, err)
	} else {
		log.Printf("Set affinity=%s", cpus)
	}
}

// getCPUNumber возвращает номер CPU, на котором выполняется процесс
func getCPUNumber() int {
	// Читаем из /proc/self/stat
	data, err := os.ReadFile("/proc/self/stat")
	if err != nil {
		return -1
	}

	// Парсим строку - 39-е поле в /proc/self/stat это processor number
	fields := strings.Fields(string(data))
	if len(fields) < 39 {
		return -1
	}

	cpu, err := strconv.Atoi(fields[38])
	if err != nil {
		return -1
	}

	return cpu
}

// getCurrentAffinity возвращает текущую CPU affinity
func getCurrentAffinity() string {
	var cpuSet unix.CPUSet
	err := unix.SchedGetaffinity(0, &cpuSet)
	if err != nil {
		return "unknown"
	}

	var cpus []int
	for i := 0; i < 1024; i++ {
		if cpuSet.IsSet(i) {
			cpus = append(cpus, i)
		}
	}

	if len(cpus) == 0 {
		return "none"
	}

	return fmt.Sprintf("%v", cpus)
}

func main() {
	var workerID string
	var configPath string
	var nice int
	var cpus string

	flag.StringVar(&workerID, "id", "0", "Worker ID")
	flag.StringVar(&configPath, "config", "config.json", "Config file path")
	flag.IntVar(&nice, "nice", 0, "Nice value")
	flag.StringVar(&cpus, "cpus", "", "CPU affinity")
	flag.Parse()

	config, err := loadConfig(configPath)
	if err != nil {
		log.Fatalf("Failed to load config: %v", err)
	}

	// Настройка обработчиков сигналов
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGTERM, syscall.SIGUSR1, syscall.SIGUSR2)

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

	// Устанавливаем начальный режим
	mode = config.ModeDefault
	if mode == "" {
		mode = "heavy"
	}

	pid := os.Getpid()

	// Применяем параметры планирования
	if nice != 0 {
		trySetNice(nice)
	}
	if cpus != "" {
		trySetAffinity(cpus)
	}

	// Получаем текущую affinity для логирования
	currentAffinity := getCurrentAffinity()

	log.Printf("[worker %s] Started, PID=%d, mode=%s, nice=%d, affinity=%s",
		workerID, pid, mode, nice, currentAffinity)

	// Главный рабочий цикл
	for !stopFlag {
		var workUS, sleepUS int

		// Получаем настройки текущего профиля
		if mode == "heavy" {
			workUS = config.ModeHeavy.WorkUS
			sleepUS = config.ModeHeavy.SleepUS
		} else {
			workUS = config.ModeLight.WorkUS
			sleepUS = config.ModeLight.SleepUS
		}

		// Значения по умолчанию если не заданы в конфиге
		if workUS == 0 {
			if mode == "heavy" {
				workUS = 9000
				sleepUS = 1000
			} else {
				workUS = 2000
				sleepUS = 8000
			}
		}

		// Имитация работы
		busyWaitMicros(workUS)

		statsTick++

		// Получаем номер CPU
		cpuNum := getCPUNumber()

		// Логируем только каждую 10-ю итерацию
		if statsTick%10 == 0 {
			log.Printf("[worker %s] tick=%d mode=%s work_us=%d sleep_us=%d cpu=%d",
				workerID, statsTick, mode, workUS, sleepUS, cpuNum)
		}

		// Сон
		time.Sleep(time.Duration(sleepUS) * time.Microsecond)
	}

	log.Printf("[worker %s] Exiting cleanly (processed %d ticks)", workerID, statsTick)
	os.Exit(0)
}
