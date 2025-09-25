#!/usr/bin/env python3
"""
worker.py
Простой worker, который:
 - принимает --id и --config
 - выполняет цикл: busy loop work_us + sleep sleep_us (в микросекундах)
 - реагирует на SIGTERM (корректный выход), SIGUSR1 (switch to LIGHT), SIGUSR2 (switch to HEAVY)
 - печатает диагностические строки (PID, mode, tick, current CPU)
"""

import argparse
import json
import os
import signal
import sys
import time
from threading import Event

terminate_event = Event()
mode_lock = None

class Worker:
    def __init__(self, wid, cfgpath):
        self.wid = wid
        self.cfgpath = cfgpath
        self.tick = 0
        self.mode = 'heavy'  # default
        self.load_params = {}
        self.last_print = 0
        self.read_config()

    def read_config(self):
        try:
            with open(self.cfgpath, 'r') as f:
                cfg = json.load(f)
        except Exception as e:
            cfg = {}
        # load mode params
        heavy = cfg.get('mode_heavy', {})
        light = cfg.get('mode_light', {})
        # fallback defaults
        self.load_params['heavy_work_us'] = int(heavy.get('work_us', cfg.get('heavy_work_us', 9000)))
        self.load_params['heavy_sleep_us'] = int(heavy.get('sleep_us', cfg.get('heavy_sleep_us', 1000)))
        self.load_params['light_work_us'] = int(light.get('work_us', cfg.get('light_work_us', 2000)))
        self.load_params['light_sleep_us'] = int(light.get('sleep_us', cfg.get('light_sleep_us', 8000)))
        # allow override by per-worker settings
        self.load_params['id'] = self.wid

    def set_mode_light(self, *_):
        self.mode = 'light'
        print(f"[worker {os.getpid()}] SIGUSR1 -> switch to LIGHT", flush=True)

    def set_mode_heavy(self, *_):
        self.mode = 'heavy'
        print(f"[worker {os.getpid()}] SIGUSR2 -> switch to HEAVY", flush=True)

    def on_terminate(self, *_):
        print(f"[worker {os.getpid()}] SIGTERM received -> terminating gracefully", flush=True)
        terminate_event.set()

    def busy_loop(self, seconds):
        # simple CPU-bound busy loop for approximate seconds
        end = time.time() + seconds
        # do some work that isn't optimized away
        x = 0
        while time.time() < end and not terminate_event.is_set():
            x += 1
            # keep under control (no extra syscalls)
        return x

    def diag(self):
        print(f"[worker {os.getpid()}] mode={self.mode} tick={self.tick}", flush=True)

    def loop(self):
        signal.signal(signal.SIGTERM, self.on_terminate)
        signal.signal(signal.SIGUSR1, lambda s,f: self.set_mode_light())
        signal.signal(signal.SIGUSR2, lambda s,f: self.set_mode_heavy())
        # main
        while not terminate_event.is_set():
            self.tick += 1
            # decide work/sleep based on mode
            if self.mode == 'heavy':
                work_us = self.load_params.get('heavy_work_us', 9000)
                sleep_us = self.load_params.get('heavy_sleep_us', 1000)
            else:
                work_us = self.load_params.get('light_work_us', 2000)
                sleep_us = self.load_params.get('light_sleep_us', 8000)

            # busy for work_us
            self.busy_loop(work_us / 1_000_000.0)
            # occasional diag print
            if time.time() - self.last_print >= 2.0:
                self.diag()
                self.last_print = time.time()
            # sleep
            # use small sleeps to be responsive to signals
            slept = 0.0
            target = sleep_us / 1_000_000.0
            while slept < target and not terminate_event.is_set():
                to_sleep = min(0.1, target - slept)
                time.sleep(to_sleep)
                slept += to_sleep
        print(f"[worker {os.getpid()}] exiting (tick={self.tick})", flush=True)
        sys.exit(0)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--id', type=int, default=0)
    parser.add_argument('--config', default='config.json')
    args = parser.parse_args()
    w = Worker(args.id, args.config)
    w.loop()

if __name__ == '__main__':
    main()
