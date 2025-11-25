#!/usr/bin/env python3
import os
import sys
import signal
import time
import subprocess
from typing import List, Dict
from dataclasses import dataclass

# Проверка версии Python
if sys.version_info < (3, 6):
    print("Error: Python 3.6 or higher is required")
    sys.exit(1)

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False
    print("Warning: psutil not found, using fallback process checking")

# Локальный импорт config после проверки версии
from config import SupervisorConfig

@dataclass
class WorkerInfo:
    pid: int
    worker_id: int
    start_time: float
    restart_count: int = 0
    last_restart: float = 0

class Supervisor:
    def __init__(self, config_file: str):
        self.config_file = config_file
        self.config = SupervisorConfig.from_file(config_file)
        self.workers: Dict[int, WorkerInfo] = {}
        self.shutdown_requested = False
        self.reload_requested = False
        self.restart_history: List[float] = []
        self.setup_signal_handlers()

    def setup_signal_handlers(self):
        """Установка обработчиков сигналов"""
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.sigchld_handler)

    def signal_handler(self, signum, frame):
        """Обработчик основных сигналов"""
        if signum in (signal.SIGTERM, signal.SIGINT):
            self.log(f"Received signal {signum}, initiating graceful shutdown")
            self.shutdown_requested = True
        elif signum == signal.SIGHUP:
            self.log("Received SIGHUP, initiating graceful reload")
            self.reload_requested = True
        elif signum == signal.SIGUSR1:
            self.log("Received SIGUSR1, switching workers to LIGHT mode")
            self.broadcast_signal(signal.SIGUSR1)
        elif signum == signal.SIGUSR2:
            self.log("Received SIGUSR2, switching workers to HEAVY mode")
            self.broadcast_signal(signal.SIGUSR2)

    def sigchld_handler(self, signum, frame):
        """Обработчик SIGCHLD для сбора информации о завершенных процессах"""
        try:
            while True:
                # Сбор информации о завершенных дочерних процессах
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break

                # Поиск воркера по PID
                worker_id = None
                for wid, winfo in self.workers.items():
                    if winfo.pid == pid:
                        worker_id = wid
                        break

                if worker_id is not None:
                    self.log(f"Worker {worker_id} (PID: {pid}) exited with status {status}")
                    if worker_id in self.workers:
                        del self.workers[worker_id]

                    # Проверка частоты рестартов
                    if not self.shutdown_requested and self.can_restart():
                        self.log(f"Restarting worker {worker_id}")
                        self.start_worker(worker_id)
        except ChildProcessError:
            pass
        except Exception as e:
            self.log(f"Error in SIGCHLD handler: {e}")

    def can_restart(self) -> bool:
        """Проверка возможности рестарта (не более 5 рестартов за 30 секунд)"""
        now = time.time()
        self.restart_history = [t for t in self.restart_history if now - t <= 30]

        if len(self.restart_history) >= 5:
            self.log("Restart rate limit exceeded (5 restarts in 30 seconds)")
            return False

        self.restart_history.append(now)
        return True

    def start_worker(self, worker_id: int) -> bool:
        """Запуск воркера"""
        try:
            # Получаем путь к текущему скрипту
            current_dir = os.path.dirname(os.path.abspath(__file__))
            worker_script = os.path.join(current_dir, 'worker.py')

            # Запуск воркера как отдельного процесса
            proc = subprocess.Popen([
                sys.executable, worker_script, 
                str(worker_id), self.config_file
            ], cwd=current_dir)

            self.workers[worker_id] = WorkerInfo(
                pid=proc.pid,
                worker_id=worker_id,
                start_time=time.time()
            )

            self.log(f"Started worker {worker_id} with PID {proc.pid}")
            return True
        except Exception as e:
            self.log(f"Failed to start worker {worker_id}: {e}")
            return False

    def is_process_alive(self, pid: int) -> bool:
        """Проверка жив ли процесс"""
        if HAS_PSUTIL:
            return psutil.pid_exists(pid)
        else:
            # Fallback метод без psutil
            try:
                os.kill(pid, 0)  # Отправка сигнала 0 для проверки существования
                return True
            except (ProcessLookupError, PermissionError):
                return False

    def broadcast_signal(self, sig: int):
        """Отправка сигнала всем воркерам"""
        dead_workers = []
        for worker_id, winfo in self.workers.items():
            try:
                os.kill(winfo.pid, sig)
                self.log(f"Sent signal {sig} to worker {worker_id} (PID: {winfo.pid})")
            except ProcessLookupError:
                self.log(f"Worker {worker_id} (PID: {winfo.pid}) not found")
                dead_workers.append(worker_id)
            except Exception as e:
                self.log(f"Error sending signal to worker {worker_id}: {e}")

        # Удаляем мертвых воркеров
        for worker_id in dead_workers:
            if worker_id in self.workers:
                del self.workers[worker_id]

    def graceful_shutdown(self, timeout: int = 5):
        """Корректное завершение работы"""
        self.log(f"Initiating graceful shutdown (timeout: {timeout}s)")

        # Отправка SIGTERM всем воркерам
        self.broadcast_signal(signal.SIGTERM)

        # Ожидание завершения воркеров
        start_time = time.time()
        while time.time() - start_time < timeout and self.workers:
            time.sleep(0.1)

        # Принудительное завершение оставшихся процессов
        if self.workers:
            self.log("Timeout reached, forcing shutdown")
            self.broadcast_signal(signal.SIGKILL)

            # Даем время для завершения
            time.sleep(0.5)

        self.log("Supervisor shutdown complete")

    def graceful_reload(self):
        """Корректная перезагрузка конфигурации"""
        self.log("Initiating graceful reload")

        try:
            # Перезагрузка конфигурации
            new_config = SupervisorConfig.from_file(self.config_file)
            old_worker_count = self.config.workers
            self.config = new_config

            self.log(f"New config loaded: {new_config.workers} workers")

            # Завершение лишних воркеров
            dead_workers = []
            for worker_id, winfo in self.workers.items():
                if worker_id >= new_config.workers:
                    self.log(f"Stopping excess worker {worker_id}")
                    try:
                        os.kill(winfo.pid, signal.SIGTERM)
                        dead_workers.append(worker_id)
                    except ProcessLookupError:
                        dead_workers.append(worker_id)

            # Удаляем завершенных воркеров
            for worker_id in dead_workers:
                if worker_id in self.workers:
                    del self.workers[worker_id]

            # Запуск новых воркеров
            for worker_id in range(new_config.workers):
                if worker_id not in self.workers:
                    self.start_worker(worker_id)

            self.reload_requested = False
            self.log("Graceful reload complete")

        except Exception as e:
            self.log(f"Error during reload: {e}")

    def log(self, message: str):
        """Логирование супервизора"""
        timestamp = time.strftime("%H:%M:%S")
        print(f"[{timestamp}] Supervisor (PID: {os.getpid()}): {message}")

    def run(self):
        """Основной цикл супервизора"""
        self.log(f"Supervisor started with {self.config.workers} workers")

        # Запуск начальных воркеров
        for i in range(self.config.workers):
            self.start_worker(i)

        # Основной цикл
        while not self.shutdown_requested:
            try:
                if self.reload_requested:
                    self.graceful_reload()

                # Проверка состояния воркеров (fallback)
                current_workers = list(self.workers.keys())
                for worker_id in current_workers:
                    if worker_id not in self.workers:
                        continue

                    winfo = self.workers[worker_id]
                    if not self.is_process_alive(winfo.pid):
                        self.log(f"Worker {worker_id} (PID: {winfo.pid}) died unexpectedly")
                        if worker_id in self.workers:
                            del self.workers[worker_id]
                        if not self.shutdown_requested and self.can_restart():
                            self.start_worker(worker_id)

                time.sleep(1)

            except KeyboardInterrupt:
                self.shutdown_requested = True
                self.log("Received KeyboardInterrupt")
            except Exception as e:
                self.log(f"Unexpected error in main loop: {e}")
                time.sleep(1)

        # Завершение работы
        self.graceful_shutdown()

def main():
    """Точка входа супервизора"""
    if len(sys.argv) != 2:
        print("Usage: supervisor.py <config_file>")
        sys.exit(1)

    config_file = sys.argv[1]

    try:
        supervisor = Supervisor(config_file)
        supervisor.run()
    except KeyboardInterrupt:
        print("\nSupervisor interrupted by user")
    except Exception as e:
        print(f"Supervisor error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()