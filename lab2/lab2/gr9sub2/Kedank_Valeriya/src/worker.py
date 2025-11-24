import os, signal, time, json, sys
import threading

mode = {}
mode_name = "heavy"
running = True
tick = 0

def load_config(path):
    with open(path) as f:
        return json.load(f)

def get_cpu():
    try:
        with open("/proc/self/stat") as f:
            fields = f.read().split()
            return fields[38] 
    except:
        return "?"

def handle_signal(signum, frame):
    global running, mode_name
    if signum == signal.SIGTERM:
        running = False
    elif signum == signal.SIGUSR1:
        mode_name = "light"
    elif signum == signal.SIGUSR2:
        mode_name = "heavy"

def report():
    while running:
        cpu = get_cpu()
        print(f"[Worker PID={os.getpid()}] Mode={mode_name} Tick={tick} CPU={cpu}")
        time.sleep(1)

if __name__ == "__main__":
    config = load_config(sys.argv[1])
    mode = config
    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGUSR1, handle_signal)
    signal.signal(signal.SIGUSR2, handle_signal)

    threading.Thread(target=report, daemon=True).start()

    while running:
        tick += 1
        m = mode[f"mode_{mode_name}"]
        time.sleep(m["work_us"] / 1_000_000)
        time.sleep(m["sleep_us"] / 1_000_000)

    print(f"[Worker PID={os.getpid()}] Exiting gracefully.")

