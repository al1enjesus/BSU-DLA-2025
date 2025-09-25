#!/usr/bin/env python3
# supervisor.py — минимальный супервизор для лабораторной работы Б

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from collections import deque
from threading import Event

ROOT = os.path.dirname(os.path.abspath(__file__))

# ---------- config utilities ----------
def load_config(path):
    try:
        import yaml
        with open(path, 'r') as f:
            return yaml.safe_load(f)
    except Exception:
        with open(path, 'r') as f:
            text = f.read().strip()
        try:
            return json.loads(text)
        except Exception:
            cfg = {}
            for line in text.splitlines():
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if ':' in line and '{' in line:
                    continue
                if '=' in line:
                    k, v = line.split('=', 1)
                    cfg[k.strip()] = v.strip()
            return cfg

# ---------- Supervisor implementation ----------
class Supervisor:
    RESTART_WINDOW = 30.0   # seconds
    RESTART_LIMIT = 5       # max restarts in window

    def __init__(self, cfg_path):
        self.cfg_path = cfg_path
        self.cfg = load_config(cfg_path)
        self.workers = {}        # pid -> {'id': id, 'popen': Popen}
        self.id_to_restarts = {} # worker_id -> deque(timestamps)
        self.stop_event = Event()
        self.graceful_timeout = 5.0
        self.next_worker_id = 0

        # register handlers
        signal.signal(signal.SIGCHLD, self._on_sigchld)
        signal.signal(signal.SIGTERM, self._on_terminate)
        signal.signal(signal.SIGINT, self._on_terminate)
        signal.signal(signal.SIGHUP, self._on_hup)
        signal.signal(signal.SIGUSR1, self._on_usr1)
        signal.signal(signal.SIGUSR2, self._on_usr2)

    # ---------- helper for setting nice ----------
    @staticmethod
    def nice_preexec(nice_value):
        def _func():
            try:
                os.nice(nice_value)
            except Exception as e:
                print(f"[worker] Failed to set nice: {e}")
        return _func

    # spawn worker (optionally keep same logical id)
    def spawn_worker(self, worker_id=None):
        if worker_id is None:
            worker_id = self.next_worker_id
            self.next_worker_id += 1

        worker_py = os.path.join(ROOT, "worker.py")
        cmd = [sys.executable, worker_py, "--config", os.path.abspath(self.cfg_path)]

        # установить nice: четные воркеры — стандартный приоритет, нечетные — пониженный
        nice_value = 0 if worker_id % 2 == 0 else 10

        p = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            text=True,
            preexec_fn=self.nice_preexec(nice_value)
        )

        self.workers[p.pid] = {"id": worker_id, "popen": p, "nice": nice_value}
        self.id_to_restarts.setdefault(worker_id, deque())
        self.id_to_restarts[worker_id].append(time.time())

        print(f"[supervisor] spawned worker id={worker_id} pid={p.pid} nice={nice_value}")
        return p.pid

    def start_initial(self):
        n = int(self.cfg.get("workers", 3))
        for _ in range(n):
            self.spawn_worker()

    # ---------- signal handlers ----------
    def _on_sigchld(self, signum, frame):
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                self._handle_child_exit(pid, status)
        except ChildProcessError:
            pass

    def _handle_child_exit(self, pid, status):
        meta = self.workers.pop(pid, None)
        if meta is None:
            print(f"[supervisor] reaped unknown child pid={pid}")
            return

        worker_id = meta["id"]
        exitcode = status >> 8
        term_sig = status & 0xff
        print(f"[supervisor] worker id={worker_id} pid={pid} exited (exitcode={exitcode}, sig={term_sig})")

        if self.stop_event.is_set():
            return

        dq = self.id_to_restarts.setdefault(worker_id, deque())
        now = time.time()
        while dq and (now - dq[0]) > self.RESTART_WINDOW:
            dq.popleft()
        if len(dq) >= self.RESTART_LIMIT:
            print(f"[supervisor] worker id={worker_id} exceeded restart limit. NOT restarting.")
            return

        print(f"[supervisor] restarting worker id={worker_id}...")
        time.sleep(0.1)
        self.spawn_worker(worker_id=worker_id)

    def _broadcast(self, sig):
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass

    def _on_usr1(self, signum, frame):
        print("[supervisor] SIGUSR1 received: broadcasting to workers (light mode)")
        self._broadcast(signal.SIGUSR1)

    def _on_usr2(self, signum, frame):
        print("[supervisor] SIGUSR2 received: broadcasting to workers (heavy mode)")
        self._broadcast(signal.SIGUSR2)

    def _on_hup(self, signum, frame):
        print("[supervisor] SIGHUP received: reload config and restart workers gracefully")
        try:
            self.cfg = load_config(self.cfg_path)
        except Exception as e:
            print(f"[supervisor] failed to reload config: {e}")
            return

        desired = int(self.cfg.get("workers", 3))
        print(f"[supervisor] new desired workers = {desired}")
        self._broadcast(signal.SIGTERM)
        deadline = time.time() + self.graceful_timeout
        while self.workers and time.time() < deadline:
            time.sleep(0.1)
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, signal.SIGKILL)
            except Exception:
                pass
        self.workers.clear()
        for _ in range(desired):
            self.spawn_worker()

    def _on_terminate(self, signum, frame):
        print("[supervisor] termination signal received, shutting down gracefully...")
        self.stop_event.set()
        self._broadcast(signal.SIGTERM)
        deadline = time.time() + self.graceful_timeout
        while self.workers and time.time() < deadline:
            time.sleep(0.1)
            try:
                while True:
                    pid, status = os.waitpid(-1, os.WNOHANG)
                    if pid == 0:
                        break
                    self._handle_child_exit(pid, status)
            except ChildProcessError:
                break
        for pid in list(self.workers.keys()):
            try:
                print(f"[supervisor] killing stubborn worker pid={pid}")
                os.kill(pid, signal.SIGKILL)
            except Exception:
                pass
        self.workers.clear()
        print("[supervisor] shutdown complete.")
        sys.exit(0)

    def run(self):
        print("[supervisor] starting up...")
        self.start_initial()
        try:
            while not self.stop_event.is_set():
                for pid, meta in list(self.workers.items()):
                    p = meta["popen"]
                    if p.stdout:
                        line = p.stdout.readline()
                        if line:
                            print(f"[worker {meta['id']}/{pid} nice={meta['nice']}] {line.strip()}")
                time.sleep(0.2)
        except KeyboardInterrupt:
            self._on_terminate(signal.SIGINT, None)

# ---------- entrypoint ----------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config", help="path to config file (yaml/json/KEY=VALUE)")
    args = parser.parse_args()
    sup = Supervisor(args.config)
    sup.run()

if __name__ == "__main__":
    main()

