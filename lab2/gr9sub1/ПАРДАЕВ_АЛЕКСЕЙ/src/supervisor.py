#!/usr/bin/env python3
"""
supervisor.py
Запускает N воркеров (worker.py), обрабатывает сигналы:
 SIGTERM/SIGINT - graceful shutdown (посылает SIGTERM воркерам, ждёт <=5s)
 SIGHUP - reload config and gracefully restart workers
 SIGUSR1 - broadcast LIGHT mode (sent to workers)
 SIGUSR2 - broadcast HEAVY mode
Обрабатывает SIGCHLD и ре-спавнит упавшие воркеры с rate limiting
"""

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from collections import defaultdict, deque

# Constants for restart limiting
MAX_RESTARTS = 5
RESTART_WINDOW = 30.0  # seconds
GRACEFUL_TIMEOUT = 5.0  # seconds

class Supervisor:
    def __init__(self, config_path, workers_count):
        self.config_path = config_path
        self.workers_count = workers_count
        self.workers = {}  # id -> subprocess.Popen
        self.restart_history = defaultdict(lambda: deque())  # id -> deque of timestamps
        self.terminating = False
        self.config = self.read_config()
        self.pending_reload = False

        # Setup signal handlers
        signal.signal(signal.SIGTERM, self._on_terminate)
        signal.signal(signal.SIGINT, self._on_terminate)
        signal.signal(signal.SIGHUP, self._on_sighup)
        signal.signal(signal.SIGUSR1, self._on_sigusr1)
        signal.signal(signal.SIGUSR2, self._on_sigusr2)
        signal.signal(signal.SIGCHLD, self._on_sigchld)

    def read_config(self):
        try:
            with open(self.config_path, 'r') as f:
                cfg = json.load(f)
            return cfg
        except Exception as e:
            print(f"[supervisor] Failed to read config {self.config_path}: {e}", file=sys.stderr)
            return {}

    def spawn_worker(self, wid):
        """Spawn a worker process with id wid and apply nice/affinity if present"""
        cmd = [sys.executable, os.path.join(os.path.dirname(__file__), 'worker.py'),
               '--id', str(wid), '--config', self.config_path]
        env = os.environ.copy()
        # You can pass extra env vars if needed
        p = subprocess.Popen(cmd, env=env)
        pid = p.pid
        print(f"[supervisor] Spawned worker id={wid} pid={pid}")
        # apply nice if configured
        try:
            nice_list = self.config.get('nice_map', [])
            if len(nice_list) > wid and hasattr(os, 'setpriority'):
                val = int(nice_list[wid])
                os.setpriority(os.PRIO_PROCESS, pid, val)
                print(f"[supervisor] setpriority pid={pid} -> {val}")
        except Exception as e:
            print(f"[supervisor] failed setpriority for pid={pid}: {e}")
        # apply affinity if configured
        try:
            aff_map = self.config.get('affinity_map', [])
            if len(aff_map) > wid and hasattr(os, 'sched_setaffinity'):
                cpus = aff_map[wid]
                if isinstance(cpus, list) and len(cpus) > 0:
                    mask = set(int(x) for x in cpus)
                    os.sched_setaffinity(pid, mask)
                    print(f"[supervisor] sched_setaffinity pid={pid} -> {mask}")
        except Exception as e:
            print(f"[supervisor] failed setaffinity for pid={pid}: {e}")
        self.workers[wid] = p
        return p

    def start_all(self):
        for i in range(self.workers_count):
            self.spawn_worker(i)

    def _on_terminate(self, signum, frame):
        if self.terminating:
            return
        print(f"[supervisor] received termination signal {signum}, shutting down workers...")
        self.terminating = True
        self.shutdown_graceful()

    def shutdown_graceful(self):
        # send SIGTERM to all workers
        for wid, p in list(self.workers.items()):
            try:
                print(f"[supervisor] -> SIGTERM worker id={wid} pid={p.pid}")
                p.send_signal(signal.SIGTERM)
            except Exception as e:
                print(f"[supervisor] error signaling worker id={wid}: {e}")
        # wait up to GRACEFUL_TIMEOUT for all to exit
        deadline = time.time() + GRACEFUL_TIMEOUT
        while time.time() < deadline and any(p.poll() is None for p in self.workers.values()):
            time.sleep(0.1)
        # force kill remaining
        for wid, p in list(self.workers.items()):
            if p.poll() is None:
                print(f"[supervisor] worker id={wid} pid={p.pid} did not exit in time -> SIGKILL")
                try:
                    p.kill()
                except Exception:
                    pass
        print("[supervisor] shutdown complete")
        # ensure children reaped
        self._reap_all()
        sys.exit(0)

    def _on_sighup(self, signum, frame):
        print("[supervisor] received SIGHUP -> scheduling reload")
        # perform reload in main loop
        self.pending_reload = True

    def _on_sigusr1(self, signum, frame):
        print("[supervisor] received SIGUSR1 -> broadcasting LIGHT mode")
        # broadcast to workers
        for p in list(self.workers.values()):
            try:
                p.send_signal(signal.SIGUSR1)
            except Exception as e:
                print(f"[supervisor] send SIGUSR1 failed: {e}")

    def _on_sigusr2(self, signum, frame):
        print("[supervisor] received SIGUSR2 -> broadcasting HEAVY mode")
        for p in list(self.workers.values()):
            try:
                p.send_signal(signal.SIGUSR2)
            except Exception as e:
                print(f"[supervisor] send SIGUSR2 failed: {e}")

    def _on_sigchld(self, signum, frame):
        # reap quickly
        # NOTE: avoid heavy work in signal handler; just set flag and reap non-blocking here
        # We will reap here safely using waitpid with WNOHANG in a loop
        try:
            while True:
                # -1 means any child
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                # find which worker this was
                wid = None
                for k, p in list(self.workers.items()):
                    if p.pid == pid:
                        wid = k
                        break
                if wid is not None:
                    ret = status >> 8
                    print(f"[supervisor] worker id={wid} pid={pid} exited with code={ret}")
                    # remove from map
                    self.workers.pop(wid, None)
                    # decide whether to restart
                    if not self.terminating:
                        self.maybe_restart_worker(wid)
        except ChildProcessError:
            # no children
            pass
        except Exception as e:
            print(f"[supervisor] SIGCHLD handler error: {e}")

    def maybe_restart_worker(self, wid):
        now = time.time()
        dq = self.restart_history[wid]
        dq.append(now)
        # remove old timestamps
        while dq and now - dq[0] > RESTART_WINDOW:
            dq.popleft()
        if len(dq) > MAX_RESTARTS:
            print(f"[supervisor] worker id={wid} exceeded restart rate ({len(dq)} in {RESTART_WINDOW}s). Not restarting.")
            return
        print(f"[supervisor] restarting worker id={wid} (restart count this window={len(dq)})")
        self.spawn_worker(wid)

    def _reap_all(self):
        # try to reap any remaining
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
        except ChildProcessError:
            pass

    def do_reload(self):
        print("[supervisor] reloading config and gracefully restarting workers")
        new_cfg = self.read_config()
        if not new_cfg:
            print("[supervisor] reload failed: config read error, skipping")
            self.pending_reload = False
            return
        self.config = new_cfg
        # Graceful restart: for each worker, send SIGTERM and spawn replacement
        old_workers = list(self.workers.items())
        for wid, p in old_workers:
            try:
                print(f"[supervisor] graceful restart: SIGTERM worker id={wid} pid={p.pid}")
                p.send_signal(signal.SIGTERM)
            except Exception as e:
                print(f"[supervisor] error signaling worker id={wid}: {e}")
            # wait small window for exit
            deadline = time.time() + GRACEFUL_TIMEOUT
            while time.time() < deadline and p.poll() is None:
                time.sleep(0.05)
            if p.poll() is None:
                print(f"[supervisor] worker id={wid} pid={p.pid} did not exit -> kill")
                try:
                    p.kill()
                except Exception:
                    pass
            # ensure removed and spawn new
            self.workers.pop(wid, None)
            self.spawn_worker(wid)
        self.pending_reload = False
        print("[supervisor] reload complete")

    def loop(self):
        self.start_all()
        try:
            while True:
                time.sleep(0.5)
                if self.pending_reload:
                    self.do_reload()
                # If terminating, break (shutdown handled already)
                if self.terminating:
                    break
        except KeyboardInterrupt:
            self._on_terminate(signal.SIGINT, None)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', default='config.json', help='path to config json')
    parser.add_argument('-n', '--num', type=int, default=3, help='number of workers (>=2)')
    args = parser.parse_args()

    sup = Supervisor(args.config, max(2, args.num))
    sup.loop()

if __name__ == '__main__':
    main()
