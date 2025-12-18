package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"sync"
	"syscall"
	"time"
)

type Config struct {
	Workers     int               `json:"workers"`
	ModeDefault string            `json:"mode_default"`
	ModeHeavy   ModeConfig        `json:"mode_heavy"`
	ModeLight   ModeConfig        `json:"mode_light"`
	WorkerAttrs []WorkerAttr      `json:"worker_attrs"`
	NiceMap     map[string]int    `json:"nice_map"`
	AffinityMap map[string]string `json:"affinity_map"`
}

type ModeConfig struct {
	WorkUS  int `json:"work_us"`
	SleepUS int `json:"sleep_us"`
}

type WorkerAttr struct {
	Nice int    `json:"nice"`
	CPUs string `json:"cpus"`
}

type Supervisor struct {
	config       *Config
	workers      map[int]*exec.Cmd
	restartTimes map[int][]time.Time
	mutex        sync.Mutex
	terminate    bool
	reload       bool
	switchMode   string
}

func NewSupervisor(configPath string, workers int) (*Supervisor, error) {
	config, err := loadConfig(configPath)
	if err != nil {
		return nil, err
	}

	if workers > 0 {
		config.Workers = workers
	}

	return &Supervisor{
		config:       config,
		workers:      make(map[int]*exec.Cmd),
		restartTimes: make(map[int][]time.Time),
	}, nil
}

func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}

	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		return nil, fmt.Errorf("failed to parse config JSON: %w", err)
	}

	// Валидация конфига
	if config.Workers < 2 {
		config.Workers = 2
		log.Printf("Warning: workers count increased to minimum: %d", config.Workers)
	}

	return &config, nil
}

func (s *Supervisor) Start() error {
	// Настройка обработчиков сигналов
	s.setupSignals()

	log.Printf("Starting %d workers", s.config.Workers)

	// Запуск начальных воркеров
	for i := 0; i < s.config.Workers; i++ {
		s.startWorker(i)
	}

	return s.mainLoop()
}

func (s *Supervisor) setupSignals() {
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh,
		syscall.SIGTERM, // graceful shutdown
		syscall.SIGINT,  // Ctrl+C
		syscall.SIGHUP,  // reload config
		syscall.SIGUSR1, // switch to light mode
		syscall.SIGUSR2, // switch to heavy mode
		syscall.SIGCHLD, // child process status change
	)

	go func() {
		for sig := range sigCh {
			switch sig {
			case syscall.SIGTERM, syscall.SIGINT:
				log.Printf("Received %v -> initiating graceful shutdown", sig)
				s.terminate = true
			case syscall.SIGHUP:
				log.Println("Received SIGHUP -> reloading configuration")
				s.reload = true
			case syscall.SIGUSR1:
				log.Println("Received SIGUSR1 -> switching all workers to LIGHT mode")
				s.switchMode = "light"
			case syscall.SIGUSR2:
				log.Println("Received SIGUSR2 -> switching all workers to HEAVY mode")
				s.switchMode = "heavy"
			case syscall.SIGCHLD:
				s.reapWorkers()
			}
		}
	}()
}

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
		log.Printf("Worker %d restart rate limit exceeded (%d restarts in 30s) - skipping restart", id, len(recent))
		return
	}

	// Построение команды для воркера
	workerPath := "./worker"
	if _, err := os.Stat(workerPath); os.IsNotExist(err) {
		// Если worker нет в текущей директории, ищем рядом с супервизором
		workerPath = filepath.Join(filepath.Dir(os.Args[0]), "worker")
	}

	cmd := exec.Command(workerPath,
		"--id", fmt.Sprintf("%d", id),
		"--config", "config/config.json")

	// Применяем атрибуты воркера
	attrs := s.getWorkerAttrs(id)
	if attrs.Nice != 0 {
		cmd.Args = append(cmd.Args, "--nice", fmt.Sprintf("%d", attrs.Nice))
	}
	if attrs.CPUs != "" {
		cmd.Args = append(cmd.Args, "--cpus", attrs.CPUs)
	}

	// Настраиваем стандартные потоки
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		log.Printf("Failed to start worker %d: %v", id, err)
		return
	}

	s.workers[id] = cmd
	s.restartTimes[id] = append(recent, now)

	log.Printf("Started worker %d with PID %d (nice=%d, cpus=%s)",
		id, cmd.Process.Pid, attrs.Nice, attrs.CPUs)
}

func (s *Supervisor) getWorkerAttrs(id int) WorkerAttr {
	// Сначала проверяем worker_attrs массив
	if id < len(s.config.WorkerAttrs) {
		return s.config.WorkerAttrs[id]
	}

	// Fallback к мапам
	attr := WorkerAttr{}
	idStr := fmt.Sprintf("%d", id)
	if nice, ok := s.config.NiceMap[idStr]; ok {
		attr.Nice = nice
	}
	if cpus, ok := s.config.AffinityMap[idStr]; ok {
		attr.CPUs = cpus
	}

	return attr
}

func (s *Supervisor) reapWorkers() {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	// Используем неблокирующий wait для сбора всех завершенных детей
	for {
		var status syscall.WaitStatus
		pid, err := syscall.Wait4(-1, &status, syscall.WNOHANG, nil)
		if err != nil || pid <= 0 {
			break // Нет больше завершенных процессов
		}

		// Находим какой воркер завершился
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

func (s *Supervisor) broadcastSignal(sig syscall.Signal) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	log.Printf("Broadcasting signal %v to all workers", sig)
	count := 0
	for id, cmd := range s.workers {
		if cmd.Process != nil {
			log.Printf("Sending signal %v to worker %d (PID %d)", sig, id, cmd.Process.Pid)
			if err := cmd.Process.Signal(sig); err != nil {
				log.Printf("Failed to send signal %v to worker %d: %v", sig, id, err)
			} else {
				count++
			}
		}
	}
	log.Printf("Sent signal %v to %d workers", sig, count)
}

func (s *Supervisor) gracefulShutdown() {
	log.Println("Initiating graceful shutdown...")

	// Отправляем SIGTERM всем воркерам
	s.broadcastSignal(syscall.SIGTERM)

	// Ждем завершения процессов
	timeout := time.After(5 * time.Second)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	log.Println("Waiting for workers to exit gracefully...")

	for range ticker.C {
		allStopped := true
		s.mutex.Lock()
		for id, cmd := range s.workers {
			if cmd.Process != nil {
				if err := cmd.Process.Signal(syscall.Signal(0)); err == nil {
					allStopped = false
					log.Printf("Worker %d (PID %d) still running...", id, cmd.Process.Pid)
				} else {
					// Процесс завершился
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
			log.Println("Timeout reached - forcing shutdown of remaining workers")
			s.broadcastSignal(syscall.SIGKILL)
			return
		default:
		}
	}

	log.Println("Graceful shutdown completed")
}

func (s *Supervisor) reloadConfig() {
    log.Println("=== RELOAD CONFIGURATION ===")
    
    // ОТЛАДКА: покажем текущие PID до reload
    s.mutex.Lock()
    currentPIDs := make(map[int]int)
    for i, cmd := range s.workers {
        if cmd.Process != nil {
            currentPIDs[i] = cmd.Process.Pid
        }
    }
    s.mutex.Unlock()
    log.Printf("BEFORE RELOAD - Current worker PIDs: %v", currentPIDs)

    // 1. Загрузить новую конфигурацию (ИСПРАВЛЕННЫЙ ПУТЬ)
    configPath := "config/config.json" // Правильный путь к конфигу
    log.Printf("Loading config from: %s", configPath)
    
    config, err := loadConfig(configPath)
    if err != nil {
        log.Printf("Failed to reload config from %s: %v", configPath, err)
        return
    }

    log.Printf("New configuration: workers=%d (old=%d), mode_default=%s",
        config.Workers, len(currentPIDs), config.ModeDefault)

    // 2. АГРЕССИВНО убиваем всех текущих воркеров
    s.mutex.Lock()
    log.Printf("Killing all %d workers", len(s.workers))
    
    for i, cmd := range s.workers {
        if cmd.Process != nil {
            oldPID := cmd.Process.Pid
            log.Printf("Sending SIGKILL to worker %d (PID %d)", i, oldPID)
            // Немедленное убийство, не ждем graceful shutdown
            cmd.Process.Signal(syscall.SIGKILL)
        }
    }
    
    // ПОЛНОСТЬЮ очищаем мапы
    s.workers = make(map[int]*exec.Cmd)
    s.restartTimes = make(map[int][]time.Time)
    s.mutex.Unlock()

    // 3. Ждем чтобы процессы точно умерли
    log.Println("Waiting for processes to die...")
    time.Sleep(1 * time.Second)

    // 4. Запускаем НОВЫХ воркеров
    log.Printf("Starting %d NEW workers", config.Workers)
    newPIDs := make(map[int]int)
    
    for i := 0; i < config.Workers; i++ {
        s.startWorker(i)
        
        // Собираем новые PID
        s.mutex.Lock()
        if cmd, exists := s.workers[i]; exists && cmd.Process != nil {
            newPIDs[i] = cmd.Process.Pid
            log.Printf("Started worker %d with NEW PID: %d", i, cmd.Process.Pid)
        }
        s.mutex.Unlock()
    }

    // 5. Детальная проверка
    log.Printf("=== RELOAD COMPLETED ===")
    log.Printf("Old PIDs: %v", currentPIDs)
    log.Printf("New PIDs: %v", newPIDs)
    
    // Проверяем изменения
    changed := false
    for i := range newPIDs {
        if oldPID, wasRunning := currentPIDs[i]; wasRunning {
            if newPIDs[i] != oldPID {
                changed = true
                log.Printf("PID CHANGED: worker %d: %d -> %d", i, oldPID, newPIDs[i])
            } else {
                log.Printf("PID UNCHANGED: worker %d: %d", i, oldPID)
            }
        }
    }
    
    if changed {
        log.Println("SUCCESS: Worker PIDs changed after reload!")
    } else {
        log.Println("PROBLEM: Worker PIDs did not change - reload may not be working")
    }
}

func (s *Supervisor) mainLoop() error {
	ticker := time.NewTicker(200 * time.Millisecond)
	defer ticker.Stop()

	log.Println("Supervisor main loop started")

	for {
		select {
		case <-ticker.C:
			// Проверка умерших воркеров (fallback)
			s.reapWorkers()

			// Обработка переключения режимов
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

			// Обработка перезагрузки конфигурации
			if s.reload {
				s.reloadConfig()
				s.reload = false
			}

			// Обработка завершения работы
			if s.terminate {
				s.gracefulShutdown()
				return nil
			}
		}
	}
}
