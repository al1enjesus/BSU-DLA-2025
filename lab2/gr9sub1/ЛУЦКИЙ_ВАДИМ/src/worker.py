#!/usr/bin/env python3
import os
import time
import signal
import sys
import json

class Worker:
    def __init__(self, worker_id, config_file):
        self.worker_id = worker_id
        self.config_file = config_file
        self.running = True
        self.current_mode = "heavy"
        self.last_print_time = 0
        self.print_interval = 3  # Печатать каждые 3 секунды
        self.load_config()
        
        # Установка обработчиков сигналов
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        
    def load_config(self):
        """Загрузка конфигурации из файла"""
        try:
            with open(self.config_file, 'r') as f:
                config = json.load(f)
            self.mode_heavy = config['mode_heavy']
            self.mode_light = config['mode_light']
        except Exception as e:
            print(f"Worker {self.worker_id}: Error loading config: {e}")
            self.mode_heavy = {"work_us": 9000, "sleep_us": 1000}
            self.mode_light = {"work_us": 2000, "sleep_us": 8000}
    
    def signal_handler(self, signum, frame):
        """Обработчик сигналов"""
        if signum == signal.SIGTERM:
            print(f"Worker {self.worker_id} (PID {os.getpid()}): Received SIGTERM, shutting down")
            self.running = False
        elif signum == signal.SIGUSR1:
            print(f"Worker {self.worker_id} (PID {os.getpid()}): Received SIGUSR1, switching to light mode")
            self.current_mode = "light"
        elif signum == signal.SIGUSR2:
            print(f"Worker {self.worker_id} (PID {os.getpid()}): Received SIGUSR2, switching to heavy mode")
            self.current_mode = "heavy"
    
    def get_process_info(self):
        """Получение информации о процессе"""
        try:
            # Информация о CPU
            with open('/proc/self/stat', 'r') as f:
                stat_data = f.read().split()
                cpu_num = stat_data[38] if len(stat_data) > 38 else 'unknown'
            
            # Nice значение
            nice = os.nice(0)  # Получаем текущее nice значение
            
            # CPU affinity (маска процессоров)
            if hasattr(os, 'sched_getaffinity'):
                affinity = os.sched_getaffinity(0)
                affinity_str = str(sorted(affinity))
            else:
                affinity_str = "not supported"
                
            return cpu_num, nice, affinity_str
            
        except Exception as e:
            return 'unknown', 'unknown', f'error: {e}'
    
    def work_cycle(self):
        """Один цикл работы воркера"""
        # Получаем параметры для текущего режима
        if self.current_mode == "heavy":
            work_time = self.mode_heavy['work_us'] / 1_000_000
            sleep_time = self.mode_heavy['sleep_us'] / 1_000_000
        else:
            work_time = self.mode_light['work_us'] / 1_000_000
            sleep_time = self.mode_light['sleep_us'] / 1_000_000
        
        # Имитация работы
        start = time.time()
        while time.time() - start < work_time:
            pass
        
        # Печатаем информацию не чаще чем раз в print_interval секунд
        current_time = time.time()
        if current_time - self.last_print_time >= self.print_interval:
            cpu_num, nice, affinity = self.get_process_info()
            print(f"Worker {self.worker_id} (PID {os.getpid()}): "
                  f"mode={self.current_mode}, cpu={cpu_num}, nice={nice}, "
                  f"affinity={affinity}, work={work_time:.3f}s, sleep={sleep_time:.3f}s")
            self.last_print_time = current_time
        
        # Сон
        time.sleep(sleep_time)
    
    def run(self):
        """Основной цикл воркера"""
        cpu_num, nice, affinity = self.get_process_info()
        print(f"Worker {self.worker_id} (PID {os.getpid()}): Started in {self.current_mode} mode")
        print(f"Worker {self.worker_id}: nice={nice}, affinity={affinity}")
        
        while self.running:
            self.work_cycle()
        
        print(f"Worker {self.worker_id} (PID {os.getpid()}): Stopped")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: worker.py <worker_id> <config_file>")
        sys.exit(1)
    
    worker_id = sys.argv[1]
    config_file = sys.argv[2]
    
    worker = Worker(worker_id, config_file)
    worker.run()