#!/usr/bin/env python3
# supervisor.py — минимальный супервизор для лабораторной работы

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
    # try YAML first, fallback to JSON/simple parser
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

    # spawn worker (optionally keep same logical id)
    def spawn_worker(self, worker_id=None):
        if worker_id is None:
            worker_id = self.next_worker_id
            self.next_worker_id += 1

        worker_py = os.path.join(ROOT, "worker.py")
        cmd = [sys.executable, worker_py, "--config", os.path.abspath(self.cfg_path)]
        # start_new_session so child is in its own session (safer)
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True, text=True)

        self.workers[p.pid] = {"id": worker_id, "popen": p}
        self.id_to_restarts.setdefault(worker_id, deque())
        # record a "start" timestamp for restart-limiting logic
        self.id_to_restarts[worker_id].append(time.time())

        print(f"[supervisor] spawned worker id={worker_id} pid={p.pid}")
        return p.pid

    def start_initial(self):
        n = int(self.cfg.get("workers", 3))
        for _ in range(n):
            self.spawn_worker()

    # signal handlers
    def _on_sigchld(self, signum, frame):
        # reap all dead children
        try:
            while True:
                # -1 => any child, WNOHANG => non-blocking
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
            # we're shutting down; don't restart
            return

        # check restart rate limit
        dq = self.id_to_restarts.setdefault(worker_id, deque())
        now = time.time()
        # drop old timestamps
        while dq and (now - dq[0]) > self.RESTART_WINDOW:
            dq.popleft()
        if len(dq) >= self.RESTART_LIMIT:
            print(f"[supervisor] worker id={worker_id} exceeded restart limit ({self.RESTART_LIMIT} times in {self.RESTART_WINDOW}s). NOT restarting.")
            return

        # restart
        print(f"[supervisor] restarting worker id={worker_id}...")
        time.sleep(0.1)  # small backoff
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

        # graceful restart: ask workers to stop, then start new ones to match new config
        desired = int(self.cfg.get("workers", 3))
        print(f"[supervisor] new desired workers = {desired}")
        # send SIGTERM to all workers
        self._broadcast(signal.SIGTERM)
        # wait up to graceful_timeout for them to exit
        deadline = time.time() + self.graceful_timeout
        while self.workers and time.time() < deadline:
            time.sleep(0.1)
        # kill any remaining
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, signal.SIGKILL)
            except Exception:
                pass
        self.workers.clear()
        # spawn desired count
        for _ in range(desired):
            self.spawn_worker()

    def _on_terminate(self, signum, frame):
        print("[supervisor] termination signal received, shutting down gracefully...")
        self.stop_event.set()
        # ask workers to exit
        self._broadcast(signal.SIGTERM)
        # give them time
        deadline = time.time() + self.graceful_timeout
        while self.workers and time.time() < deadline:
            time.sleep(0.1)
            # reap any exited children (SIGCHLD handler will run too)
            # but explicitly try to reap here as well
            try:
                while True:
                    pid, status = os.waitpid(-1, os.WNOHANG)
                    if pid == 0:
                        break
                    self._handle_child_exit(pid, status)
            except ChildProcessError:
                break

        # force kill remaining
        for pid in list(self.workers.keys()):
            try:
                print(f"[supervisor] killing stubborn worker pid={pid}")
                os.kill(pid, signal.SIGKILL)
            except Exception:
                pass
        self.workers.clear()
        print("[supervisor] shutdown complete.")
        # exit process
        sys.exit(0)

    def run(self):
        print("[supervisor] starting up...")
        self.start_initial()
        # main loop — keep the process alive; signal handlers do the work
        try:
            while not self.stop_event.is_set():
                # try to read worker outputs (non-blocking)
                for pid, meta in list(self.workers.items()):
                    p = meta["popen"]
                    if p.stdout:
                        # read available lines without blocking
                        line = p.stdout.readline()
                        if line:
                            print(f"[worker {meta['id']}/{pid}] {line.strip()}")
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

