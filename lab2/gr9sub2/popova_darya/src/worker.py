#!/usr/bin/env python3
import argparse
import json
import os
import signal
import sys
import time
from pathlib import Path

try:
    import psutil
except Exception:
    psutil = None

stop_flag = False
mode = "heavy"  # default until config read
stats_tick = 0


def handle_sigterm(signum, frame):
    global stop_flag
    print(f"[worker {os.getpid()}] got SIGTERM -> will stop after current cycle")
    stop_flag = True


def handle_sigusr1(signum, frame):
    global mode
    mode = "light"
    print(f"[worker {os.getpid()}] switched to LIGHT mode (SIGUSR1)")


def handle_sigusr2(signum, frame):
    global mode
    mode = "heavy"
    print(f"[worker {os.getpid()}] switched to HEAVY mode (SIGUSR2)")


def load_config(path):
    with open(path, "r") as f:
        return json.load(f)


def busy_wait_micros(us):
    # simple busy loop to simulate CPU work (fine for demo)
    start = time.perf_counter()
    end = start + us / 1_000_000.0
    while time.perf_counter() < end:
        pass


def try_set_nice(n):
    try:
        # PRIO_PROCESS, pid 0 (self)
        os.setpriority(os.PRIO_PROCESS, 0, int(n))
        print(f"[worker {os.getpid()}] set nice={n}")
    except Exception as e:
        print(f"[worker {os.getpid()}] cannot set nice={n}: {e}")


def try_set_affinity(cpus):
    try:
        # cpus: iterable of ints
        cpu_set = {int(c) for c in cpus}
        os.sched_setaffinity(0, cpu_set)
        print(f"[worker {os.getpid()}] set affinity={sorted(cpu_set)}")
    except Exception as e:
        print(f"[worker {os.getpid()}] cannot set affinity={cpus}: {e}")


def main():
    global mode, stats_tick, psutil
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", required=True)
    ap.add_argument("--config", default="config.json")
    ap.add_argument("--nice", type=int, help="nice value to set for this worker")
    ap.add_argument("--cpus", type=str, help="comma-separated cpu list to pin to, e.g. 0,1")
    args = ap.parse_args()
    config_path = Path(args.config)

    # register signals
    signal.signal(signal.SIGTERM, handle_sigterm)
    signal.signal(signal.SIGUSR1, handle_sigusr1)
    signal.signal(signal.SIGUSR2, handle_sigusr2)

    if psutil is None:
        print("[worker] WARNING: psutil not installed — CPU number reporting will be disabled")

    cfg = load_config(config_path)
    # initial mode from config is optional
    mode = "heavy" if cfg.get("mode_default", "heavy") == "heavy" else "light"

    pid = os.getpid()
    proc = psutil.Process(pid) if psutil else None

    # apply requested scheduling attributes early
    if args.nice is not None:
        try_set_nice(args.nice)

    if args.cpus:
        cpus_list = [int(x.strip()) for x in args.cpus.split(",") if x.strip() != ""]
        if cpus_list:
            try_set_affinity(cpus_list)

    try:
        affinity = os.sched_getaffinity(0)
    except Exception:
        affinity = None

    print(f"[worker {pid}] started, mode={mode}, affinity={affinity}")

    # main loop
    while not stop_flag:
        # determine current profile
        profile = cfg.get("mode_heavy") if mode == "heavy" else cfg.get("mode_light")

        # small safe fallback (in case profile is missing)
        if profile is None:
            if mode == "heavy":
                work_us = 9000
                sleep_us = 1000
            else:
                work_us = 2000
                sleep_us = 8000
        else:
            work_us = int(profile.get("work_us", 5000))
            sleep_us = int(profile.get("sleep_us", 1000))

        # simulate work
        busy_wait_micros(work_us)

        # stats
        stats_tick += 1
        cpu_now = None
        if proc:
            try:
                cpu_now = proc.cpu_num()
            except Exception:
                cpu_now = None
        print(f"[worker {pid}] tick={stats_tick} mode={mode} work_us={work_us} sleep_us={sleep_us} cpu={cpu_now}")

        # cooperative sleep
        time.sleep(sleep_us / 1_000_000.0)

    print(f"[worker {pid}] exiting cleanly (processed {stats_tick} ticks)")


if __name__ == "__main__":
    main()

