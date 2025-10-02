import os
import json
import signal
import subprocess
import threading
import psutil

sigchld_lock = threading.Lock()
workers = []

def sigchld_handler(signum, frame):
    with sigchld_lock:
        try:
            while True:
                pid, _ = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                print(f"[SUPERVISOR] Процесс {pid} завершён")
        except ChildProcessError:
            pass

signal.signal(signal.SIGCHLD, sigchld_handler)

try:
    with open("config.json") as f:
        config = json.load(f)
except (FileNotFoundError, json.JSONDecodeError) as e:
    print(f"[SUPERVISOR] Ошибка загрузки конфигурации: {e}")
    config = {}

nice_value = config.get("nice", 0)
cpu_core = config.get("cpu_core", 0)
cpu_count = psutil.cpu_count()

if cpu_core >= cpu_count:
    print(f"[SUPERVISOR] Ядро {cpu_core} не существует. Используем ядро 0.")
    cpu_core = 0

for i in range(config.get("num_workers", 3)):
    proc = subprocess.Popen(["python3", "worker.py"])
    os.setpriority(os.PRIO_PROCESS, proc.pid, nice_value)
    try:
        psutil.Process(proc.pid).cpu_affinity([cpu_core])
    except Exception as e:
        print(f"[SUPERVISOR] Ошибка назначения CPU-аффинити: {e}")
    workers.append(proc)

for proc in workers:
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        print(f"[SUPERVISOR] Процесс {proc.pid} не завершился — принудительно завершаем")
        proc.kill()

