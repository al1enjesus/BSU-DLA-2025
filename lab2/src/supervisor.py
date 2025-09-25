#!/usr/bin/env python3
import os, signal, sys, time, multiprocessing

running = True
mode = "heavy"

def worker(mode_heavy, mode_light):
    global mode, running
    pid = os.getpid()
    work, sleep = mode_heavy
    print(f"[Worker {pid}] started in heavy mode")

    def sigterm_handler(signum, frame):
        global running
        running = False
        print(f"[Worker {pid}] exiting gracefully")

    def sigusr1_handler(signum, frame):
        nonlocal work, sleep
        work, sleep = mode_light
        print(f"[Worker {pid}] switched to light mode")

    def sigusr2_handler(signum, frame):
        nonlocal work, sleep
        work, sleep = mode_heavy
        print(f"[Worker {pid}] switched to heavy mode")

    signal.signal(signal.SIGTERM, sigterm_handler)
    signal.signal(signal.SIGUSR1, sigusr1_handler)
    signal.signal(signal.SIGUSR2, sigusr2_handler)

    while running:
        # имитация вычислений
        t_end = time.time() + work / 1e6
        while time.time() < t_end:
            pass
        time.sleep(sleep / 1e6)

def supervisor():
    workers = []
    mode_heavy = (9000, 1000)   # work_us, sleep_us
    mode_light = (2000, 8000)

    def sigterm_handler(signum, frame):
        print("[Supervisor] SIGTERM → shutting down workers")
        for w in workers:
            os.kill(w.pid, signal.SIGTERM)
        for w in workers:
            w.join(timeout=5)
        sys.exit(0)

    def sighup_handler(signum, frame):
        print("[Supervisor] SIGHUP → reload config + restart workers")
        for w in workers:
            os.kill(w.pid, signal.SIGTERM)
            w.join()
        workers.clear()
        start_workers()

    def start_workers():
        for _ in range(3):
            w = multiprocessing.Process(target=worker, args=(mode_heavy, mode_light))
            w.start()
            workers.append(w)

    signal.signal(signal.SIGTERM, sigterm_handler)
    signal.signal(signal.SIGHUP, sighup_handler)

    start_workers()
    print("[Supervisor] started")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        sigterm_handler(signal.SIGINT, None)

if __name__ == "__main__":
    supervisor()
