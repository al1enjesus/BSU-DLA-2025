import os
import sys
import time
import signal
import subprocess
import json
from datetime import datetime, timedelta

class WorkerManager:
    def __init__(self, config_file="config_B.json"):
        self.workers = {}
        self.config = self.load_config(config_file)
        self.restart_times = []
        self.shutdown_requested = False
        self.reload_requested = False
        self.current_mode = "heavy"

        # Настройка обработчиков сигналов
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)

        print(f"Supervisor PID: {os.getpid()}")
        print(f"Config: {self.config}")

    def load_config(self, config_file):
        """Загрузка конфигурации с параметрами планирования"""
        default_config = {
            "workers": 4,
            "mode_heavy": {"work_us": 900000, "sleep_us": 100000},  # 0.9s work, 0.1s sleep
            "mode_light": {"work_us": 200000, "sleep_us": 800000},  # 0.2s work, 0.8s sleep
            "nice_levels": [0, 10, 0, 10],  # Уровни nice для каждого воркера
            "cpu_affinity": [[0], [1], [0], [1]]  # Закрепление за CPU ядрами
        }
        
        try:
            with open(config_file, 'r') as f:
                config = json.load(f)
            return config
        except (FileNotFoundError, json.JSONDecodeError):
            return default_config

    def signal_handler(self, signum, frame):
        if signum in (signal.SIGTERM, signal.SIGINT):
            print(f"Received signal {signum}, initiating graceful shutdown...")
            self.shutdown_requested = True
        elif signum == signal.SIGHUP:
            print("Received SIGHUP, initiating graceful reload...")
            self.reload_requested = True
        elif signum == signal.SIGUSR1:
            print("Received SIGUSR1, switching to light mode...")
            self.current_mode = "light"
            self.broadcast_signal(signal.SIGUSR1)
        elif signum == signal.SIGUSR2:
            print("Received SIGUSR2, switching to heavy mode...")
            self.current_mode = "heavy"
            self.broadcast_signal(signal.SIGUSR2)
        elif signum == signal.SIGCHLD:
            self.handle_child_exit()

    def broadcast_signal(self, sig):
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                if pid in self.workers:
                    del self.workers[pid]

    def handle_child_exit(self):
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                if pid in self.workers:
                    worker_info = self.workers[pid]
                    print(f"Worker {worker_info['index']} exited with status {status}")
                    del self.workers[pid]
                    if not self.shutdown_requested and self.can_restart():
                        print(f"Restarting worker {worker_info['index']}...")
                        self.start_worker(worker_info['index'])
        except ChildProcessError:
            pass

    def can_restart(self):
        now = datetime.now()
        self.restart_times = [t for t in self.restart_times if now - t < timedelta(seconds=30)]
        if len(self.restart_times) >= 5:
            print("Restart rate limit exceeded, waiting...")
            return False
        self.restart_times.append(now)
        return True

    def start_worker(self, index):
        try:
            # Передаем параметры планирования через переменные окружения
            env = os.environ.copy()
            env.update({
                'WORKER_INDEX': str(index),
                'WORKER_MODE': self.current_mode,
                'WORKER_NICE': str(self.config['nice_levels'][index]),
                'WORKER_CPU_AFFINITY': ','.join(map(str, self.config['cpu_affinity'][index]))
            })
            
            proc = subprocess.Popen([
                sys.executable, __file__, '--worker', str(index), '--mode', self.current_mode
            ], env=env)
            
            self.workers[proc.pid] = {'process': proc, 'index': index}
            print(f"Started worker {index} with PID {proc.pid}, "
                  f"nice: {self.config['nice_levels'][index]}, "
                  f"CPU affinity: {self.config['cpu_affinity'][index]}")
            
        except Exception as e:
            print(f"Failed to start worker {index}: {e}")

    def graceful_shutdown(self):
        print("Sending SIGTERM to all workers...")
        self.broadcast_signal(signal.SIGTERM)
        
        start_time = time.time()
        while self.workers and (time.time() - start_time) < 5:
            time.sleep(0.1)
            for pid in list(self.workers.keys()):
                if not self.is_process_running(pid):
                    del self.workers[pid]
        
        if self.workers:
            print("Forcing termination of remaining workers...")
            self.broadcast_signal(signal.SIGKILL)
            time.sleep(0.5)
        
        print("All workers terminated")

    def graceful_reload(self):
        print("Reloading configuration...")
        self.config = self.load_config("config_scheduling.json")
        
        print("Gracefully restarting workers...")
        self.broadcast_signal(signal.SIGTERM)
        time.sleep(1)
        
        for i in range(self.config['workers']):
            self.start_worker(i)
        
        self.reload_requested = False
        print("Reload completed")

    def is_process_running(self, pid):
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False

    def start_all_workers(self):
        print("Starting workers with scheduling parameters:")
        for i in range(self.config['workers']):
            self.start_worker(i)

    def run(self):
        print("Starting supervisor with scheduling...")
        self.start_all_workers()
        
        try:
            while not self.shutdown_requested:
                time.sleep(1)
                
                if self.reload_requested:
                    self.graceful_reload()
                
                current_count = len(self.workers)
                expected_count = self.config['workers']
                if current_count < expected_count and not self.shutdown_requested:
                    running_indices = [w['index'] for w in self.workers.values()]
                    for i in range(expected_count):
                        if i not in running_indices and self.can_restart():
                            self.start_worker(i)
                            
        except KeyboardInterrupt:
            self.shutdown_requested = True
        finally:
            self.graceful_shutdown()


class Worker:
    def __init__(self, worker_id, mode):
        self.worker_id = worker_id
        self.mode = mode
        self.running = True
        self.ticks = 0
        self.start_time = time.time()
        
        # Получаем параметры планирования из переменных окружения
        self.nice_level = int(os.environ.get('WORKER_NICE', '0'))
        cpu_affinity_str = os.environ.get('WORKER_CPU_AFFINITY', '0')
        self.cpu_affinity = [int(x) for x in cpu_affinity_str.split(',')] if cpu_affinity_str else [0]
        
        # Применяем параметры планирования
        self.apply_scheduling()
        
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

    def apply_scheduling(self):
        """Применяем nice и CPU affinity"""
        try:
            # Устанавливаем nice level
            if hasattr(os, 'nice'):
                current_nice = os.nice(0)  # Получаем текущий nice
                new_nice = os.nice(self.nice_level - current_nice)
                print(f"Worker {self.worker_id} nice level set to {new_nice}")
            
            # Устанавливаем CPU affinity
            if hasattr(os, 'sched_setaffinity'):
                os.sched_setaffinity(0, self.cpu_affinity)
                actual_affinity = os.sched_getaffinity(0)
                print(f"Worker {self.worker_id} CPU affinity set to {actual_affinity}")
                
        except Exception as e:
            print(f"Worker {self.worker_id} scheduling error: {e}")

    def signal_handler(self, signum, frame):
        if signum == signal.SIGTERM:
            print(f"Worker {self.worker_id} received shutdown signal")
            self.running = False
        elif signum == signal.SIGUSR1:
            self.mode = "light"
            print(f"Worker {self.worker_id} switched to light mode")
        elif signum == signal.SIGUSR2:
            self.mode = "heavy"
            print(f"Worker {self.worker_id} switched to heavy mode")

    def work_cycle(self):
        config = {
            "mode_heavy": {"work_us": 900000, "sleep_us": 100000},
            "mode_light": {"work_us": 200000, "sleep_us": 800000}
        }
        
        while self.running:
            self.ticks += 1
            
            work_time = config[f"mode_{self.mode}"]["work_us"] / 1000000
            sleep_time = config[f"mode_{self.mode}"]["sleep_us"] / 1000000
            
            # Вывод статуса с информацией о планировании
            if self.ticks % 5 == 0:
                current_cpu = self.get_current_cpu()
                print(f"Worker {self.worker_id} [PID:{os.getpid()}] - "
                      f"Mode: {self.mode}, Nice: {self.nice_level}, "
                      f"CPU: {current_cpu}, Affinity: {self.cpu_affinity}, "
                      f"Ticks: {self.ticks}")
            
            time.sleep(work_time)
            time.sleep(sleep_time)
        
        print(f"Worker {self.worker_id} shutting down")

    def get_current_cpu(self):
        """Получаем текущий CPU где выполняется процесс"""
        try:
            if hasattr(os, 'sched_getaffinity'):
                cpus = os.sched_getaffinity(0)
                return min(cpus) if cpus else 0
            return 0
        except:
            return 0


def worker_main(worker_id, mode):
    worker = Worker(worker_id, mode)
    print(f"Worker {worker_id} started with PID {os.getpid()}, mode: {mode}")
    worker.work_cycle()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == '--worker':
        worker_id = int(sys.argv[2])
        mode = sys.argv[4] if len(sys.argv) > 4 else 'heavy'
        worker_main(worker_id, mode)
    else:
        config_file = sys.argv[1] if len(sys.argv) > 1 else "config_scheduling.json"
        manager = WorkerManager(config_file)
        manager.run()