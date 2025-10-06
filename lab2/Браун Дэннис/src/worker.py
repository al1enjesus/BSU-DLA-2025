#!/usr/bin/env python3
import os
import time
import signal
import sys
import json
from multiprocessing import current_process


class Worker:
    def __init__(self, worker_id, config):
        self.worker_id = worker_id
        self.config = config
        self.current_mode = "heavy"  # Начальный режим
        self.shutdown_requested = False
        self.work_params = config[f"mode_{self.current_mode}"]

        # Установка обработчиков сигналов
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

    def signal_handler(self, signum, frame):
        if signum == signal.SIGTERM:
            print(f"[Worker {self.worker_id}] Received SIGTERM, shutting down...")
            self.shutdown_requested = True
        elif signum == signal.SIGUSR1:
            print(f"[Worker {self.worker_id}] Switching to LIGHT mode")
            self.current_mode = "light"
            self.work_params = self.config["mode_light"]
        elif signum == signal.SIGUSR2:
            print(f"[Worker {self.worker_id}] Switching to HEAVY mode")
            self.current_mode = "heavy"
            self.work_params = self.config["mode_heavy"]

    def simulate_work(self, duration_us):
        """Имитация работы CPU"""
        end_time = time.time() + (duration_us / 1_000_000)
        while time.time() < end_time and not self.shutdown_requested:
            # Просто крутимся в цикле - это нагружает CPU
            pass

    def run(self):
        tick_count = 0
        while not self.shutdown_requested:
            # Получаем текущий CPU (если доступно)
            try:
                cpu_id = os.sched_getcpu()
            except:
                cpu_id = "N/A"

            # Выводим диагностику
            print(f"[Worker {self.worker_id}] PID: {os.getpid()}, "
                  f"Mode: {self.current_mode}, Tick: {tick_count}, "
                  f"CPU: {cpu_id}")

            # Имитация работы
            self.simulate_work(self.work_params["work_us"])

            # Пауза
            if not self.shutdown_requested:
                time.sleep(self.work_params["sleep_us"] / 1_000_000)

            tick_count += 1

        print(f"[Worker {self.worker_id}] Shutdown complete")


def main():
    if len(sys.argv) != 3:
        print("Usage: worker.py <worker_id> <config_json>")
        sys.exit(1)

    worker_id = sys.argv[1]
    config_json = sys.argv[2]

    try:
        config = json.loads(config_json)
        worker = Worker(worker_id, config)
        worker.run()
    except Exception as e:
        print(f"Worker error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
