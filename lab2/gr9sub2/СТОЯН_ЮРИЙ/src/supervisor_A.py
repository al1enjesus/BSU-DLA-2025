import os
import sys
import time
import signal
import subprocess
import json
from datetime import datetime, timedelta

class WorkerManager:
    def __init__(self, config_file="config_A.json"):
        self.workers = {}
        self.config = self.load_config(config_file)
        self.restart_times = []
        self.shutdown_requested = False
        self.reload_requested = False
        self.current_mode = "heavy"
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)

        print(f"Supervisor PID: {os.getpid()}")
        print(f"Config: {self.config}")

    def load_config(self, config_file):
        default_config = {
            "workers": 3,
            "mode_heavy": {"work_us": 9000, "sleep_us": 1000},
            "mode_light": {"work_us": 2000, "sleep_us": 8000}
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
            proc = subprocess.Popen([
                sys.executable, __file__, '--worker', str(index), '--mode', self.current_mode
            ])
            self.workers[proc.pid] = {'process': proc, 'index': index}
            print(f"Started worker {index} with PID {proc.pid}")
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
        self.config = self.load_config("config.json")
        
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
        for i in range(self.config['workers']):
            self.start_worker(i)

    def run(self):
        print("Starting supervisor...")
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
        self.cpu_number = os.getpid() % 4
        
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

    def signal_handler(self, signum, frame):
        if signum == signal.SIGTERM:
            print(f"Worker {self.worker_id} received shutdown signal")
            self.running = False
        elif signum == signal.SIGUSR1:
            self.mode = "light"
        elif signum == signal.SIGUSR2:
            self.mode = "heavy"

    def work_cycle(self):
        config = {
            "mode_heavy": {"work_us": 9000, "sleep_us": 1000},
            "mode_light": {"work_us": 2000, "sleep_us": 8000}
        }
        
        while self.running:
            self.ticks += 1
            work_time = config[f"mode_{self.mode}"]["work_us"] / 1000000
            sleep_time = config[f"mode_{self.mode}"]["sleep_us"] / 1000000
            if self.ticks % 10 == 0:
                print(f"Worker {self.worker_id} [PID:{os.getpid()}] - "
                      f"Mode: {self.mode}, Ticks: {self.ticks}, "
                      f"CPU: {self.cpu_number}, Work: {work_time:.3f}s, "
                      f"Sleep: {sleep_time:.3f}s")
            
            time.sleep(work_time)
            time.sleep(sleep_time)
        
        print(f"Worker {self.worker_id} shutting down")


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
        config_file = sys.argv[1] if len(sys.argv) > 1 else "config.json"
        manager = WorkerManager(config_file)
        manager.run()