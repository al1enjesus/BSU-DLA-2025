import os, signal, time, json, subprocess, sys
from collections import deque
import psutil  # добавлено для CPU-аффинити

CONFIG_PATH = sys.argv[1]
WORKERS = {}
RESTART_LOG = deque()

def load_config():
    with open(CONFIG_PATH) as f:
        return json.load(f)

def spawn_worker(nice_value=0, cpu_core=None):
    proc = subprocess.Popen(
        ["nice", "-n", str(nice_value), "python3", "worker.py", CONFIG_PATH]
    )
    if cpu_core is not None:
        try:
            psutil.Process(proc.pid).cpu_affinity([cpu_core])
        except Exception as e:
            print(f"[Supervisor] Failed to set CPU affinity for PID={proc.pid}: {e}")
    return proc

def restart_worker(pid):
    now = time.time()
    RESTART_LOG.append(now)
    while RESTART_LOG and now - RESTART_LOG[0] > 30:
        RESTART_LOG.popleft()
    if len(RESTART_LOG) <= 5:
        print(f"[Supervisor] Restarting worker {pid}")
        i = len(WORKERS)
        nice_val = 10 if i % 2 == 0 else 0
        cpu_core = 0 if i % 2 == 0 else 1
        new_proc = spawn_worker(nice_val, cpu_core)
        WORKERS[new_proc.pid] = new_proc
    else:
        print("[Supervisor] Restart limit reached")

def shutdown():
    print("[Supervisor] Shutting down...")
    for pid, proc in WORKERS.items():
        proc.send_signal(signal.SIGTERM)
    deadline = time.time() + 5
    while time.time() < deadline and any(p.poll() is None for p in WORKERS.values()):
        time.sleep(0.1)
    print("[Supervisor] All workers exited.")
    exit(0)

def reload():
    print("[Supervisor] Reloading config and restarting workers...")
    for pid, proc in WORKERS.items():
        proc.send_signal(signal.SIGTERM)
    time.sleep(1)
    WORKERS.clear()
    config = load_config()
    for i in range(config["workers"]):
        nice_val = 10 if i % 2 == 0 else 0
        cpu_core = 0 if i % 2 == 0 else 1
        proc = spawn_worker(nice_val, cpu_core)
        WORKERS[proc.pid] = proc

def broadcast(sig):
    for proc in WORKERS.values():
        proc.send_signal(sig)

def sig_handler(signum, frame):
    if signum in [signal.SIGINT, signal.SIGTERM]:
        shutdown()
    elif signum == signal.SIGHUP:
        reload()
    elif signum == signal.SIGUSR1:
        broadcast(signal.SIGUSR1)
    elif signum == signal.SIGUSR2:
        broadcast(signal.SIGUSR2)

def sigchld_handler(signum, frame):
    for pid, proc in list(WORKERS.items()):
        ret = proc.poll()
        if ret is not None:
            print(f"[Supervisor] Worker {pid} exited with code {ret}")
            if pid in WORKERS:
                del WORKERS[pid]
            restart_worker(pid)

if __name__ == "__main__":
    signal.signal(signal.SIGCHLD, sigchld_handler)
    signal.signal(signal.SIGTERM, sig_handler)
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGHUP, sig_handler)
    signal.signal(signal.SIGUSR1, sig_handler)
    signal.signal(signal.SIGUSR2, sig_handler)

    config = load_config()
    for i in range(config["workers"]):
        nice_val = 10 if i % 2 == 0 else 0
        cpu_core = 0 if i % 2 == 0 else 1
        proc = spawn_worker(nice_val, cpu_core)
        WORKERS[proc.pid] = proc

    print(f"[Supervisor PID={os.getpid()}] Running with {len(WORKERS)} workers.")
    while True:
        time.sleep(1)

