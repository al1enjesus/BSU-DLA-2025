#!/usr/bin/env python3
import os
import sys
import signal
import time
import json
import subprocess
from datetime import datetime, timedelta


class Supervisor:
    def __init__(self, config_file):
        self.config_file = config_file
        self.config = self.load_config()
        self.workers = {}  # worker_id -> {'process': subprocess, 'start_time': datetime}
        self.restart_history = {}  # worker_id -> list of restart times
        self.shutdown_requested = False
        self.reload_requested = False

        # Установка обработчиков сигналов
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)

    def load_config(self):
        """Загрузка конфигурации из файла"""
        try:
            with open(self.config_file, 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"Error loading config: {e}")
            sys.exit(1)

    def signal_handler(self, signum, frame):
        if signum in (signal.SIGTERM, signal.SIGINT):
            print(f"Received signal {signum}, initiating graceful shutdown...")
            self.shutdown_requested = True
        elif signum == signal.SIGHUP:
            print("Received SIGHUP, initiating graceful reload...")
            self.reload_requested = True
        elif signum == signal.SIGUSR1:
            print("Received SIGUSR1, switching all workers to LIGHT mode")
            self.broadcast_signal(signal.SIGUSR1)
        elif signum == signal.SIGUSR2:
            print("Received SIGUSR2, switching all workers to HEAVY mode")
            self.broadcast_signal(signal.SIGUSR2)
        elif signum == signal.SIGCHLD:
            # Обработка завершения дочерних процессов
            self.handle_child_exit()

    def broadcast_signal(self, sig):
        """Отправка сигнала всем воркерам"""
        for worker_id, worker_info in self.workers.items():
            if worker_info['process'].poll() is None:  # Процесс еще работает
                try:
                    os.kill(worker_info['process'].pid, sig)
                    print(f"Sent signal {sig} to worker {worker_id}")
                except ProcessLookupError:
                    print(f"Worker {worker_id} not found, might have died")

    def handle_child_exit(self):
        """Обработка завершения дочерних процессов"""
        try:
            while True:
                # Неблокирующее ожидание завершения дочерних процессов
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:  # Нет завершенных процессов
                    break

                # Находим какой воркер завершился
                for worker_id, worker_info in self.workers.items():
                    if worker_info['process'].pid == pid:
                        print(f"Worker {worker_id} (PID: {pid}) exited with status {status}")

                        # Проверяем частоту рестартов
                        if self.can_restart(worker_id):
                            print(f"Restarting worker {worker_id}")
                            self.start_worker(worker_id)
                        else:
                            print(f"Too many restarts for worker {worker_id}, not restarting")
                        break
        except ChildProcessError:
            pass  # Нет дочерних процессов

    def can_restart(self, worker_id):
        """Проверка возможности рестарта (не более 5 за 30 секунд)"""
        now = datetime.now()
        if worker_id not in self.restart_history:
            self.restart_history[worker_id] = []

        # Удаляем старые записи (старше 30 секунд)
        self.restart_history[worker_id] = [
            t for t in self.restart_history[worker_id]
            if now - t < timedelta(seconds=30)
        ]

        # Проверяем количество рестартов
        if len(self.restart_history[worker_id]) >= 5:
            return False

        self.restart_history[worker_id].append(now)
        return True

    def start_worker(self, worker_id):
        """Запуск воркера"""
        config_json = json.dumps(self.config)
        try:
            process = subprocess.Popen([
                sys.executable, 'worker.py', worker_id, config_json
            ])
            self.workers[worker_id] = {
                'process': process,
                'start_time': datetime.now()
            }
            print(f"Started worker {worker_id} with PID {process.pid}")
        except Exception as e:
            print(f"Error starting worker {worker_id}: {e}")

    def start_all_workers(self):
        """Запуск всех воркеров"""
        num_workers = self.config.get('workers', 2)
        for i in range(num_workers):
            self.start_worker(f"worker_{i + 1}")

    def graceful_shutdown(self):
        """Корректное завершение всех воркеров"""
        print("Initiating graceful shutdown...")

        # Отправляем SIGTERM всем воркерам
        self.broadcast_signal(signal.SIGTERM)

        # Ждем завершения (максимум 5 секунд)
        start_time = time.time()
        while time.time() - start_time < 5:
            all_done = True
            for worker_id, worker_info in self.workers.items():
                if worker_info['process'].poll() is None:
                    all_done = False
                    break

            if all_done:
                print("All workers shutdown successfully")
                return

            time.sleep(0.1)

        # Если время вышло - принудительно завершаем
        print("Graceful shutdown timeout, forcing termination...")
        for worker_id, worker_info in self.workers.items():
            if worker_info['process'].poll() is None:
                worker_info['process'].terminate()

    def graceful_reload(self):
        """Graceful reload - перезапуск с новым конфигом"""
        print("Initiating graceful reload...")

        # Перезагружаем конфиг
        self.config = self.load_config()
        print("Config reloaded")

        # Мягко завершаем старых воркеров
        self.broadcast_signal(signal.SIGTERM)

        # Ждем немного и запускаем новых
        time.sleep(1)
        self.start_all_workers()

    def run(self):
        """Основной цикл супервизора"""
        print(f"Supervisor started with PID {os.getpid()}")
        self.start_all_workers()

        try:
            while not self.shutdown_requested:
                # Проверяем флаг reload
                if self.reload_requested:
                    self.graceful_reload()
                    self.reload_requested = False

                # Проверяем состояние воркеров
                self.check_workers()

                # Короткая пауза
                time.sleep(0.5)

        except KeyboardInterrupt:
            print("\nReceived KeyboardInterrupt")
            self.shutdown_requested = True

        finally:
            self.graceful_shutdown()
            print("Supervisor shutdown complete")

    def check_workers(self):
        """Периодическая проверка состояния воркеров"""
        for worker_id, worker_info in self.workers.items():
            if worker_info['process'].poll() is not None:
                # Воркер завершился
                print(f"Worker {worker_id} died, checking for restart...")
                if self.can_restart(worker_id):
                    print(f"Restarting worker {worker_id}")
                    self.start_worker(worker_id)
                else:
                    print(f"Too many restarts for worker {worker_id}")


def main():
    if len(sys.argv) != 2:
        print("Usage: supervisor.py <config_file>")
        sys.exit(1)

    config_file = sys.argv[1]
    supervisor = Supervisor(config_file)
    supervisor.run()


if __name__ == "__main__":
    main()
