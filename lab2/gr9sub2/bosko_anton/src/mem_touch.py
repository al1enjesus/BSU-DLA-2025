#!/usr/bin/env python3
"""
mem_touch.py
Простая утилита для постепенного увеличения RSS (имитация роста потребления памяти).
Поддерживает:
 - --rss-mb TARGET    : цель (сколько MB удерживать)
 - --step-mb STEP     : шаг пошагового выделения (MB)
 - --sleep-ms N       : пауза между шагами (ms)
 - --limit-mb L       : (опционально) установить RLIMIT_AS в байтах до старта (ограничит адресное пространство)
Сигналы:
 - SIGUSR1 : добавить один STEP (увеличить цель на STEP)
 - SIGUSR2 : уменьшить цель на STEP (освободить последний шаг)
 - SIGTERM : graceful exit
Выводит свои VmSize/VmRSS из /proc/self/status каждый шаг.
"""

import argparse
import os
import signal
import sys
import time
import resource

chunks = []
stop_flag = False
target_mb = 0
step_mb = 64

def read_proc_status(pid):
    """Возвращает словарь ключ->значение для /proc/<pid>/status первых полезных полей."""
    status = {}
    try:
        with open(f"/proc/{pid}/status", "r") as f:
            for line in f:
                if ':' not in line:
                    continue
                k, v = line.split(':', 1)
                status[k.strip()] = v.strip()
    except FileNotFoundError:
        return {}
    return status

def print_mem(pid, note=""):
    st = read_proc_status(pid)
    vmrss = st.get("VmRSS", "n/a")
    vmsize = st.get("VmSize", "n/a")
    print(f"[mem_touch] pid={pid} target={target_mb}MB allocated_chunks={len(chunks)} step={step_mb}MB {note} | VmRSS={vmrss} VmSize={vmsize}", flush=True)

def sigterm_handler(signum, frame):
    global stop_flag
    print("[mem_touch] SIGTERM received, exiting gracefully", flush=True)
    stop_flag = True

def sigusr1_handler(signum, frame):
    global target_mb
    target_mb += step_mb
    print(f"[mem_touch] SIGUSR1: increasing target -> {target_mb}MB", flush=True)

def sigusr2_handler(signum, frame):
    global target_mb
    target_mb = max(0, target_mb - step_mb)
    # free one chunk immediately if present
    if chunks:
        freed = chunks.pop()
        del freed
    print(f"[mem_touch] SIGUSR2: decreasing target -> {target_mb}MB", flush=True)

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--rss-mb", type=int, default=128, dest="rss_mb", help="Initial target RSS (MB)")
    p.add_argument("--step-mb", type=int, default=32, dest="step_mb", help="Step to allocate per iteration (MB)")
    p.add_argument("--sleep-ms", type=int, default=200, dest="sleep_ms", help="Sleep between steps (ms)")
    p.add_argument("--limit-mb", type=int, default=0, dest="limit_mb", help="If >0, set RLIMIT_AS to this MB before allocation")
    return p.parse_args()

def set_rlimit_as(limit_mb):
    if limit_mb <= 0:
        return
    bytes_limit = limit_mb * 1024 * 1024
    try:
        resource.setrlimit(resource.RLIMIT_AS, (bytes_limit, bytes_limit))
        print(f"[mem_touch] RLIMIT_AS set to {limit_mb} MB ({bytes_limit} bytes)", flush=True)
    except Exception as e:
        print(f"[mem_touch] Failed to set RLIMIT_AS: {e}", flush=True)

def allocate_one_step(step_mb):
    # allocate but keep reference so memory is not freed
    try:
        size = step_mb * 1024 * 1024
        chunks.append(bytearray(size))
        return True
    except MemoryError:
        print("[mem_touch] MemoryError on allocation", flush=True)
        return False
    except Exception as e:
        print(f"[mem_touch] Allocation exception: {e}", flush=True)
        return False

def free_all():
    global chunks
    chunks = []
    # suggest GC
    import gc
    gc.collect()

def main():
    global target_mb, step_mb, stop_flag

    args = parse_args()
    target_mb = args.rss_mb
    step_mb = args.step_mb
    sleep_s = args.sleep_ms / 1000.0

    # optional RLIMIT_AS
    if args.limit_mb > 0:
        set_rlimit_as(args.limit_mb)

    signal.signal(signal.SIGTERM, sigterm_handler)
    signal.signal(signal.SIGINT, sigterm_handler)
    signal.signal(signal.SIGUSR1, sigusr1_handler)
    signal.signal(signal.SIGUSR2, sigusr2_handler)

    pid = os.getpid()
    print(f"[mem_touch] started pid={pid}, target={target_mb}MB step={step_mb}MB sleep={args.sleep_ms}ms", flush=True)
    print_mem(pid, note="initial")

    # initial allocation loop: allocate until we reach target
    while not stop_flag:
        allocated_mb = len(chunks) * step_mb
        if allocated_mb < target_mb:
            ok = allocate_one_step(step_mb)
            if not ok:
                # allocation failed — print diagnostics and sleep longer to avoid spinning
                print_mem(pid, note="after failed alloc")
                time.sleep(1.0)
            else:
                print_mem(pid, note="after alloc")
                time.sleep(sleep_s)
            continue
        elif allocated_mb > target_mb:
            # free one chunk
            if chunks:
                chunks.pop()
                print_mem(pid, note="after free one chunk")
            time.sleep(sleep_s)
            continue
        else:
            # allocated == target — idle until signal changes target
            print_mem(pid, note="at target (idle)")
            time.sleep(sleep_s)

    print("[mem_touch] exiting, freeing memory", flush=True)
    free_all()
    print("[mem_touch] done", flush=True)

if __name__ == "__main__":
    main()

