#!/usr/bin/env python3
import os
import sys
import time
import signal
import json
from datetime import datetime, timedelta

class Supervisor:
    def __init__(self, config_file):
        self.config_file = config_file
        self.workers = {}  # PID -> worker_info
        self.running = True
        self.restart_history = []  # История рестартов для ограничения частоты
        
        # Загрузка конфигурации
        self.load_config()
        
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
            # Обработка завершения дочерних процессов
            self.handle_child_exit()
    
    def can_restart(self):
        """Проверка возможности рестарта (не более 5 за 30 секунд)"""
        now = datetime.now()
        # Удаляем старые записи (старше 30 секунд)
        self.restart_history = [t for t in self.restart_history if now - t < timedelta(seconds=30)]
        
        # Проверяем, что не превышен лимит
        return len(self.restart_history) < 5
    
    def handle_child_exit(self):
        """Обработка завершения дочерних процессов"""
        try:
            while True:
                # Неблокирующий waitpid для сбора зомби
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:  # Нет завершённых процессов
                    break
                
                if pid in self.workers:
                    worker_info = self.workers[pid]
                    print(f"Worker {worker_info['id']} (PID {pid}) exited with status {status}")
                    
                    # Удаляем из активных процессов
                    del self.workers[pid]
                    
                    # Перезапуск, если супервизор ещё работает
                    if self.running and self.can_restart():
                        print(f"Restarting worker {worker_info['id']}")
                        self.restart_history.append(datetime.now())
                        self.start_worker(worker_info['id'])
                    else:
                        print(f"Restart limit exceeded for worker {worker_info['id']}")
                
        except ChildProcessError:
            pass  # Нет дочерних процессов
    
    def start_worker(self, worker_id):
        """Запуск воркера"""
        try:
            # Запускаем воркер как дочерний процесс
            pid = os.fork()
            
            if pid == 0:  # Дочерний процесс
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
                # Процесс уже завершился
                if pid in self.workers:
                    del self.workers[pid]
    
    def graceful_shutdown(self):
        """Корректное завершение работы"""
        print("Initiating graceful shutdown...")
        self.running = False
        
        # Отправляем SIGTERM всем воркерам
        self.broadcast_signal(signal.SIGTERM)
        
        # Ждём завершения воркеров (до 5 секунд)
        start_time = time.time()
        while self.workers and (time.time() - start_time < 5):
            time.sleep(0.1)
            # Проверяем завершившиеся процессы
            self.handle_child_exit()
        
        # Если остались работающие воркеры - отправляем SIGKILL
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
        
        # Перезагружаем конфигурацию
        old_config = self.config.copy()
        self.load_config()
        
        # Отправляем SIGTERM старым воркерам
        self.broadcast_signal(signal.SIGTERM)
        
        # Ждём их завершения
        start_time = time.time()
        while self.workers and (time.time() - start_time < 5):
            time.sleep(0.1)
            self.handle_child_exit()
        
        # Запускаем новых воркеров с новой конфигурацией
        for i in range(self.config['workers']):
            self.start_worker(i + 1)
        
        print("Graceful reload complete")
    
    def run(self):
        """Основной цикл супервизора"""
        print(f"Supervisor (PID {os.getpid()}) starting {self.config['workers']} workers...")
        
        # Запуск воркеров
        for i in range(self.config['workers']):
            self.start_worker(i + 1)
        
        print("Supervisor running. Send signals to PID", os.getpid())
        
        # Основной цикл
        while self.running:
            time.sleep(1)
            
            # Периодическая проверка состояния
            if len(self.workers) < self.config['workers'] and self.running:
                # Запускаем недостающих воркеров
                active_ids = {info['id'] for info in self.workers.values()}
                for i in range(1, self.config['workers'] + 1):
                    if i not in active_ids and self.can_restart():
                        print(f"Starting missing worker {i}")
                        self.restart_history.append(datetime.now())
                        self.start_worker(i)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: supervisor.py <config_file>")
        sys.exit(1)
    
    config_file = sys.argv[1]
    
    supervisor = Supervisor(config_file)
    supervisor.run()