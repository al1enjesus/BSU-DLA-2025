#!/usr/bin/env python3
"""
worker.py
Имитация вычислительной нагрузки для лабораторной работы DLA.
Обрабатывает сигналы от супервизора, переключается между режимами работы.
"""

import argparse
import json
import os
import signal
import sys
import time
import yaml

# --------- глобальные переменные ---------
stop_event = False
mode = "heavy"
cfg = {}

# --------- обработчики сигналов ---------
def handle_sigterm(signum, frame):
    global stop_event
    print(f"[PID {os.getpid()}] SIGTERM received, stopping gracefully.")
    stop_event = True

def handle_sigusr1(signum, frame):
    global mode
    mode = "light"
    print(f"[PID {os.getpid()}] SIGUSR1 received, switching to LIGHT mode.")

def handle_sigusr2(signum, frame):
    global mode
    mode = "heavy"
    print(f"[PID {os.getpid()}] SIGUSR2 received, switching to HEAVY mode.")

# --------- работа с конфигом ---------
def load_config(path):
    try:
        with open(path, 'r') as f:
            return yaml.safe_load(f)
    except Exception as e:
        print(f"Failed to load config: {e}")
        sys.exit(1)

# --------- имитация вычислений ---------
def busy_work(us):
    start = time.time()
    while (time.time() - start) < us / 1_000_000.0:
        pass

# --------- основной цикл ---------
def main():
    global stop_event, mode, cfg

    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True, help='Path to config.yaml')
    args = parser.parse_args()

    cfg = load_config(args.config)

    signal.signal(signal.SIGTERM, handle_sigterm)
    signal.signal(signal.SIGUSR1, handle_sigusr1)
    signal.signal(signal.SIGUSR2, handle_sigusr2)

    tick = 0
    print(f"[PID {os.getpid()}] Worker started in {mode.upper()} mode.")

    while not stop_event:
        current_cfg = cfg[f"mode_{mode}"]
        work_us = current_cfg.get("work_us", 1000)
        sleep_us = current_cfg.get("sleep_us", 1000)

        busy_work(work_us)
        tick += 1
        print(f"[PID {os.getpid()}] Tick {tick}, mode {mode.upper()}, CPU(s): {os.sched_getaffinity(0)}")
        time.sleep(sleep_us / 1_000_000.0)

    print(f"[PID {os.getpid()}] Worker exiting gracefully after {tick} ticks.")

if __name__ == "__main__":
    main()
