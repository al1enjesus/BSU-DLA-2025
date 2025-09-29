#!/usr/bin/env python3
import os
import time
import signal
import sys
import platform
from dataclasses import dataclass
from config import ModeConfig, SupervisorConfig, WorkerScheduling
from scheduler import ProcessScheduler

@dataclass
class WorkerState:
    worker_id: int
    current_mode: str = "heavy"
    shutdown_requested: bool = False
    last_tick: float = 0
    tick_count: int = 0
    current_nice: int = 0
    current_cpu_affinity: list = None

class Worker:
    def __init__(self, worker_id: int, config: SupervisorConfig):
        self.worker_id = worker_id
        self.config = config
        self.state = WorkerState(worker_id=worker_id)
        self.scheduler = ProcessScheduler()
        self.setup_signal_handlers()
        self.apply_scheduling()

    def apply_scheduling(self):
        """Применение настроек планирования при старте"""
        scheduling = self.config.get_scheduling_for_worker(self.worker_id)
        if scheduling:
            # Применяем nice
            if scheduling.nice is not None:
                if self.scheduler.set_nice(os.getpid(), scheduling.nice):
                    self.state.current_nice = scheduling.nice
                    self.log(f"Applied nice value: {scheduling.nice}")

            # Применяем CPU affinity
            if scheduling.cpu_affinity:
                if self.scheduler.set_cpu_affinity(os.getpid(), scheduling.cpu_affinity):
                    self.state.current_cpu_affinity = scheduling.cpu_affinity
                    self.log(f"Applied CPU affinity: {scheduling.cpu_affinity}")

    def setup_signal_handlers(self):
        """Установка обработчиков сигналов"""
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

    def signal_handler(self, signum, frame):
        """Обработчик сигналов"""
        if signum == signal.SIGTERM:
            self.state.shutdown_requested = True
            self.log("Received SIGTERM, shutting down gracefully")
        elif signum == signal.SIGUSR1:
            self.state.current_mode = "light"
            self.log("Switched to LIGHT mode")
        elif signum == signal.SIGUSR2:
            self.state.current_mode = "heavy"
            self.log("Switched to HEAVY mode")

    def get_current_config(self) -> ModeConfig:
        """Получение текущей конфигурации режима"""
        if self.state.current_mode == "light":
            return self.config.mode_light
        else:
            return self.config.mode_heavy

    def simulate_work(self, work_us: int):
        """Имитация работы процессора с метриками"""
        start_time = time.time()
        end_time = start_time + (work_us / 1_000_000)

        # Более интенсивная работа для лучшего измерения
        iterations = 0
        while time.time() < end_time and not self.state.shutdown_requested:
            # Интенсивные вычисления
            result = 0
            for i in range(10000):
                result += i * i
            iterations += 1

        return iterations

    def get_cpu_info(self) -> str:
        """Получение информации о CPU"""
        try:
            if platform.system() == "Linux":
                # Чтение текущего CPU из /proc
                with open('/proc/self/stat', 'r') as f:
                    stats = f.read().split()
                    if len(stats) > 38:
                        cpu_num = int(stats[38])
                        return f"CPU{cpu_num}"
            return f"PID{os.getpid() % 4}"
        except:
            return "Unknown"

    def log(self, message: str):
        """Логирование с метаинформацией о планировании"""
        timestamp = time.strftime("%H:%M:%S")
        cpu_info = self.get_cpu_info()

        affinity_info = ""
        if self.state.current_cpu_affinity:
            affinity_info = f", Affinity: {self.state.current_cpu_affinity}"

        print(f"[{timestamp}] Worker {self.worker_id} (PID: {os.getpid()}, "
              f"CPU: {cpu_info}, Nice: {self.state.current_nice}{affinity_info}, "
              f"Mode: {self.state.current_mode.upper()}, Ticks: {self.state.tick_count}): {message}")

    def run(self):
        """Основной цикл работы воркера"""
        self.log("Worker started with scheduling settings")

        while not self.state.shutdown_requested:
            mode_config = self.get_current_config()

            # Фаза работы
            self.log(f"Working for {mode_config.work_us}μs")
            iterations = self.simulate_work(mode_config.work_us)
            self.log(f"Work completed: {iterations} iterations")

            if self.state.shutdown_requested:
                break

            # Фаза отдыха
            self.log(f"Sleeping for {mode_config.sleep_us}μs")
            time.sleep(mode_config.sleep_us / 1_000_000)

            self.state.tick_count += 1

        self.log("Worker shutdown complete")

def main():
    """Точка входа для запуска воркера как отдельного процесса"""
    if len(sys.argv) != 3:
        print("Usage: worker.py <worker_id> <config_file>")
        sys.exit(1)

    worker_id = int(sys.argv[1])
    config_file = sys.argv[2]

    try:
        config = SupervisorConfig.from_file(config_file)
        worker = Worker(worker_id, config)
        worker.run()
    except Exception as e:
        print(f"Worker {worker_id} error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()