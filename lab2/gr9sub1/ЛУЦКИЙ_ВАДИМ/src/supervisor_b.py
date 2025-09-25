#!/usr/bin/env python3
import os
import sys
import time
import signal
import json
from datetime import datetime, timedelta

class SupervisorB:
    def __init__(self, config_file):
        self.config_file = config_file
        self.workers = {}
        self.running = True
        self.restart_history = []
        
        self.load_config()
        
        # Настройки планирования для задания B
        self.scheduling_config = {
            'nice_values': [0, 10, 0],  # Разные nice для воркеров
            'cpu_affinity': [[0], [1], [0, 1]]  # Разная affinity для воркеров
        }
        
        # Установка обработчиков сигналов
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)
    
    def load_config(self):
        """Загрузка конфигурации"""
        try:
            with open(self.config_file, 'r') as f:
                self.config = json.load(f)
            print("Configuration loaded successfully")
        except Exception as e:
            print(f"Error loading config: {e}")
            sys.exit(1)
    
    def set_process_scheduling(self, worker_id):
        """Установка nice и CPU affinity для процесса"""
        try:
            # Установка nice значения
            nice_value = self.scheduling_config['nice_values'][(worker_id - 1) % len(self.scheduling_config['nice_values'])]
            current_nice = os.nice(0)  # Получаем текущее значение
            new_nice = os.nice(nice_value - current_nice)  # Устанавливаем относительное значение
            print(f"Worker {worker_id}: Set nice from {current_nice} to {new_nice}")
            
            # Установка CPU affinity (только на Linux)
            if hasattr(os, 'sched_setaffinity'):
                affinity = self.scheduling_config['cpu_affinity'][(worker_id - 1) % len(self.scheduling_config['cpu_affinity'])]
                os.sched_setaffinity(0, affinity)
                print(f"Worker {worker_id}: Set CPU affinity to {affinity}")
            else:
                print(f"Worker {worker_id}: CPU affinity not supported on this system")
                
        except Exception as e:
            print(f"Error setting scheduling for worker {worker_id}: {e}")
    
    def signal_handler(self, signum, frame):
        """Обработчик сигналов"""
        if signum in (signal.SIGTERM, signal.SIGINT):
            print(f"Supervisor (PID {os.getpid()}): Received shutdown signal")
            self.graceful_shutdown()
        elif signum == signal.SIGHUP:
            print(f"Supervisor (PID {os.getpid()}): Received reload signal")
            self.graceful_reload()
        elif signum == signal.SIGUSR1:
            print(f"Supervisor (PID {os.getpid()}): Received SIGUSR1, switching workers to light mode")
            self.broadcast_signal(signal.SIGUSR1)
        elif signum == signal.SIGUSR2:
            print(f"Supervisor (PID {os.getpid()}): Received SIGUSR2, switching workers to heavy mode")
            self.broadcast_signal(signal.SIGUSR2)
        elif signum == signal.SIGCHLD:
            self.handle_child_exit()
    
    def can_restart(self):
        """Проверка возможности рестарта"""
        now = datetime.now()
        self.restart_history = [t for t in self.restart_history if now - t < timedelta(seconds=30)]
        return len(self.restart_history) < 5
    
    def handle_child_exit(self):
        """Обработка завершения дочерних процессов"""
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                
                if pid in self.workers:
                    worker_info = self.workers[pid]
                    print(f"Worker {worker_info['id']} (PID {pid}) exited with status {status}")
                    del self.workers[pid]
                    
                    if self.running and self.can_restart():
                        print(f"Restarting worker {worker_info['id']}")
                        self.restart_history.append(datetime.now())
                        self.start_worker(worker_info['id'])
                    else:
                        print(f"Restart limit exceeded for worker {worker_info['id']}")
                
        except ChildProcessError:
            pass
    
    def start_worker(self, worker_id):
        """Запуск воркера с настройками планирования"""
        try:
            pid = os.fork()
            
            if pid == 0:  # Дочерний процесс
                # Устанавливаем планирование
                self.set_process_scheduling(worker_id)
                
                # Запускаем воркер
                os.execlp(sys.executable, sys.executable, 'worker.py', 
                         str(worker_id), self.config_file)
            else:  # Родительский процесс
                self.workers[pid] = {'id': worker_id, 'start_time': time.time()}
                print(f"Started worker {worker_id} with PID {pid}")
                
        except Exception as e:
            print(f"Error starting worker {worker_id}: {e}")
    
    def broadcast_signal(self, sig):
        """Отправка сигнала всем воркерам"""
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, sig)
                print(f"Sent signal {sig} to worker PID {pid}")
            except ProcessLookupError:
                if pid in self.workers:
                    del self.workers[pid]
    
    def graceful_shutdown(self):
        """Корректное завершение работы"""
        print("Initiating graceful shutdown...")
        self.running = False
        self.broadcast_signal(signal.SIGTERM)
        
        start_time = time.time()
        while self.workers and (time.time() - start_time < 5):
            time.sleep(0.1)
            self.handle_child_exit()
        
        if self.workers:
            print("Forcing termination of remaining workers...")
            for pid in list(self.workers.keys()):
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        
        print("Supervisor shutdown complete")
        sys.exit(0)
    
    def graceful_reload(self):
        """Graceful reload конфигурации"""
        print("Initiating graceful reload...")
        old_config = self.config.copy()
        self.load_config()
        
        self.broadcast_signal(signal.SIGTERM)
        
        start_time = time.time()
        while self.workers and (time.time() - start_time < 5):
            time.sleep(0.1)
            self.handle_child_exit()
        
        for i in range(self.config['workers']):
            self.start_worker(i + 1)
        
        print("Graceful reload complete")
    
    def run(self):
        """Основной цикл супервизора"""
        print(f"Supervisor B (PID {os.getpid()}) starting {self.config['workers']} workers with scheduling...")
        print("Scheduling configuration:")
        print(f"  Nice values: {self.scheduling_config['nice_values']}")
        print(f"  CPU affinity: {self.scheduling_config['cpu_affinity']}")
        
        for i in range(self.config['workers']):
            self.start_worker(i + 1)
        
        print("Supervisor running. Send signals to PID", os.getpid())
        
        while self.running:
            time.sleep(1)
            
            if len(self.workers) < self.config['workers'] and self.running:
                active_ids = {info['id'] for info in self.workers.values()}
                for i in range(1, self.config['workers'] + 1):
                    if i not in active_ids and self.can_restart():
                        print(f"Starting missing worker {i}")
                        self.restart_history.append(datetime.now())
                        self.start_worker(i)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: supervisor_b.py <config_file>")
        sys.exit(1)
    
    config_file = sys.argv[1]
    
    supervisor = SupervisorB(config_file)
    supervisor.run()