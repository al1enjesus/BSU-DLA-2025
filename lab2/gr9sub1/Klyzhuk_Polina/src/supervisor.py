#!/usr/bin/env python3
import os
import sys
import time
import signal
import subprocess
import json
import threading
from datetime import datetime, timedelta
from pathlib import Path


class WorkerManager:
    def __init__(self, config_file="config.json"):
        self.workers = {}
        self.config = self.load_config(config_file)
        self.restart_times = []
        self.shutdown_requested = False
        self.reload_requested = False
        self.current_mode = "heavy"  # heavy/light

        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)

        print(f"Supervisor PID: {os.getpid()}")
        print(f"Config: {self.config}")

    def load_config(self, config_file):
        try:
            with open(config_file, 'r') as f:
                config = json.load(f)
            return config
        except FileNotFoundError:
            return {
                "workers": 3,
                "mode_heavy": {"work_us": 9000, "sleep_us": 1000},
                "mode_light": {"work_us": 2000, "sleep_us": 8000}
            }

    def signal_handler(self, signum, frame):
        if signum in (signal.SIGTERM, signal.SIGINT):
            print(f"\nReceived signal {signum}, initiating graceful shutdown...")
            self.shutdown_requested = True
        elif signum == signal.SIGHUP:
            print(f"\nReceived SIGHUP, initiating graceful reload...")
            self.reload_requested = True
        elif signum == signal.SIGUSR1:
            print(f"\nReceived SIGUSR1, switching to light mode...")
            self.current_mode = "light"
            self.broadcast_signal(signal.SIGUSR1)
        elif signum == signal.SIGUSR2:
            print(f"\nReceived SIGUSR2, switching to heavy mode...")
            self.current_mode = "heavy"
            self.broadcast_signal(signal.SIGUSR2)
        elif signum == signal.SIGCHLD:
            self.handle_child_exit()

    def broadcast_signal(self, sig):
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, sig)
                print(f"Sent signal {sig} to worker {pid}")
            except ProcessLookupError:
                pass

    def handle_child_exit(self):
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break

                if pid in self.workers:
                    worker_info = self.workers[pid]
                    print(f"Worker {pid} exited with status {status}")
                    del self.workers[pid]

                    if not self.shutdown_requested and self.can_restart():
                        print(f"Restarting worker {pid}...")
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
        worker_script = Path(__file__).parent / "worker.py"
        env = os.environ.copy()
        env.update({
            'WORKER_INDEX': str(index),
            'MODE_HEAVY_WORK': str(self.config['mode_heavy']['work_us']),
            'MODE_HEAVY_SLEEP': str(self.config['mode_heavy']['sleep_us']),
            'MODE_LIGHT_WORK': str(self.config['mode_light']['work_us']),
            'MODE_LIGHT_SLEEP': str(self.config['mode_light']['sleep_us']),
            'CURRENT_MODE': self.current_mode
        })

        try:
            proc = subprocess.Popen([sys.executable, str(worker_script)], env=env)
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
        old_config = self.config
        self.config = self.load_config("config.json")

        print("Gracefully restarting workers...")
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, signal.SIGTERM)
                for _ in range(50):
                    if not self.is_process_running(pid):
                        break
                    time.sleep(0.1)
            except ProcessLookupError:
                pass

        self.start_all_workers()
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
            while True:
                time.sleep(0.1)

                if self.shutdown_requested:
                    self.graceful_shutdown()
                    break

                if self.reload_requested:
                    self.graceful_reload()

                current_count = len(self.workers)
                expected_count = self.config['workers']

                if current_count < expected_count and not self.shutdown_requested:
                    for i in range(expected_count):
                        running_indices = [w['index'] for w in self.workers.values()]
                        if i not in running_indices and self.can_restart():
                            self.start_worker(i)

        except KeyboardInterrupt:
            self.shutdown_requested = True
            self.graceful_shutdown()


if __name__ == "__main__":
    config_file = sys.argv[1] if len(sys.argv) > 1 else "config.json"
    manager = WorkerManager(config_file)
    manager.run()