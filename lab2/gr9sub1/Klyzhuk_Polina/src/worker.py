import os
import sys
import time
import signal
import threading
import random


class Worker:
    def __init__(self):
        self.index = int(os.getenv('WORKER_INDEX', '0'))
        self.pid = os.getpid()

        # Параметры режимов
        self.mode_heavy = {
            'work_us': int(os.getenv('MODE_HEAVY_WORK', '9000')),
            'sleep_us': int(os.getenv('MODE_HEAVY_SLEEP', '1000'))
        }
        self.mode_light = {
            'work_us': int(os.getenv('MODE_LIGHT_WORK', '2000')),
            'sleep_us': int(os.getenv('MODE_LIGHT_SLEEP', '8000'))
        }

        self.current_mode = os.getenv('CURRENT_MODE', 'heavy')
        self.shutdown_requested = False
        self.ticks = 0
        self.work_time_total = 0
        self.sleep_time_total = 0

        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

        print(f"Worker {self.index} started with PID {self.pid}, mode: {self.current_mode}")

    def signal_handler(self, signum, frame):
        if signum in (signal.SIGTERM, signal.SIGINT):
            print(f"Worker {self.index} received shutdown signal")
            self.shutdown_requested = True
        elif signum == signal.SIGUSR1:
            print(f"Worker {self.index} switching to light mode")
            self.current_mode = 'light'
        elif signum == signal.SIGUSR2:
            print(f"Worker {self.index} switching to heavy mode")
            self.current_mode = 'heavy'

    def simulate_work(self, work_us):
        start = time.time()
        iterations = work_us // 10
        for i in range(iterations):
            _ = i * i
        return time.time() - start

    def get_current_params(self):
        if self.current_mode == 'heavy':
            return self.mode_heavy
        else:
            return self.mode_light

    def run(self):
        try:
            while not self.shutdown_requested:
                params = self.get_current_params()

                work_start = time.time()
                actual_work_time = self.simulate_work(params['work_us'])
                work_end = time.time()

                # Фаза сна
                sleep_time = params['sleep_us'] / 1_000_000.0
                time.sleep(sleep_time)
                sleep_end = time.time()

                self.ticks += 1
                self.work_time_total += actual_work_time
                self.sleep_time_total += sleep_time

                if self.ticks % 10 == 0:
                    avg_work = self.work_time_total / self.ticks
                    avg_sleep = self.sleep_time_total / self.ticks
                    cpu_id = self.get_cpu_affinity()

                    print(f"Worker {self.index} [PID:{self.pid}] - "
                          f"Mode: {self.current_mode}, Ticks: {self.ticks}, "
                          f"CPU: {cpu_id}, Work: {avg_work:.3f}s, Sleep: {avg_sleep:.3f}s")

        except Exception as e:
            print(f"Worker {self.index} error: {e}")

        print(f"Worker {self.index} shutting down")

    def get_cpu_affinity(self):
        try:
            if os.path.exists('/proc/self/stat'):
                with open('/proc/self/stat', 'r') as f:
                    stat_data = f.read().split()
                    # processor - поле 39 (0-based) в /proc/pid/stat
                    if len(stat_data) > 39:
                        return stat_data[38]
        except:
            pass
        return "N/A"


if __name__ == "__main__":
    worker = Worker()
    worker.run()