#!/usr/bin/env python3
import argparse
import json
import os
import signal
import subprocess
import sys
import time
from collections import deque, defaultdict
from pathlib import Path

RESTART_WINDOW = 30.0  # seconds
RESTART_LIMIT = 5
GRACE_TIMEOUT = 5.0  # seconds to wait for workers on shutdown


class Supervisor:
    def __init__(self, config_path, workers):
        self.config_path = Path(config_path)
        self.workers_count = max(2, int(workers))
        self.procs = {}  # index -> Popen
        self.restart_times = defaultdict(deque)  # index -> deque of timestamps
        self.terminate = False
        self.reload_requested = False
        self.switch_mode = None  # 'light' or 'heavy' or None
        self.sigchld_flag = False

        # load initial config
        self.config = self._load_config()

        # setup signal handlers
        signal.signal(signal.SIGINT, self._on_sigterm)
        signal.signal(signal.SIGTERM, self._on_sigterm)
        signal.signal(signal.SIGHUP, self._on_sighup)
        signal.signal(signal.SIGUSR1, self._on_sigusr1)
        signal.signal(signal.SIGUSR2, self._on_sigusr2)
        signal.signal(signal.SIGCHLD, self._on_sigchld)

    def _load_config(self):
        if not self.config_path.exists():
            print(f"Config not found: {self.config_path}", file=sys.stderr)
            sys.exit(1)
        with open(self.config_path, "r") as f:
            return json.load(f)

    def _on_sigterm(self, signum, frame):
        print(f"[supervisor] got SIGTERM/SIGINT ({signum}) -> shutdown")
        self.terminate = True

    def _on_sighup(self, signum, frame):
        print("[supervisor] got SIGHUP -> reload requested")
        self.reload_requested = True

    def _on_sigusr1(self, signum, frame):
        print("[supervisor] got SIGUSR1 -> switch workers to LIGHT mode")
        self.switch_mode = "light"

    def _on_sigusr2(self, signum, frame):
        print("[supervisor] got SIGUSR2 -> switch workers to HEAVY mode")
        self.switch_mode = "heavy"

    def _on_sigchld(self, signum, frame):
        # set a flag; handle reaping in main loop (safer in Python)
        self.sigchld_flag = True

    def start(self):
        print(f"[supervisor] starting {self.workers_count} workers (config: {self.config_path})")
        for i in range(self.workers_count):
            self._start_worker(i)

        try:
            self._main_loop()
        finally:
            # ensure children are terminated on unexpected exit
            self._graceful_shutdown()

    def _start_worker(self, idx):
        # check restart limit
        now = time.time()
        dq = self.restart_times[idx]
        # prune old
        while dq and now - dq[0] > RESTART_WINDOW:
            dq.popleft()
        if len(dq) >= RESTART_LIMIT:
            print(
                f"[supervisor] worker {idx} restart rate limit exceeded ({len(dq)} in {RESTART_WINDOW}s) — backoff, will not restart now"
            )
            return

        cmd = [sys.executable, str(Path(__file__).parent / "worker.py"), "--id", str(idx), "--config", str(self.config_path)]

        # pass optional nice / cpus from config if available
        attrs = {}
        wa = self.config.get("worker_attrs")
        if isinstance(wa, list) and idx < len(wa):
            attrs = wa[idx] or {}
        else:
            nice_map = self.config.get("nice_map", {})
            aff_map = self.config.get("affinity_map", {})
            if str(idx) in nice_map:
                attrs["nice"] = nice_map[str(idx)]
            if str(idx) in aff_map:
                attrs["cpus"] = aff_map[str(idx)]

        if "nice" in attrs:
            cmd += ["--nice", str(int(attrs["nice"]))]
        if "cpus" in attrs:
            if isinstance(attrs["cpus"], list):
                cpus_arg = ",".join(str(int(c)) for c in attrs["cpus"])
            else:
                cpus_arg = str(attrs["cpus"])
            cmd += ["--cpus", cpus_arg]

        p = subprocess.Popen(cmd)
        self.procs[idx] = p
        dq.append(now)
        print(f"[supervisor] started worker {idx} pid={p.pid} attrs={attrs}")

    def _stop_worker(self, idx, sig=signal.SIGTERM):
        p = self.procs.get(idx)
        if not p:
            return
        try:
            os.kill(p.pid, sig)
        except ProcessLookupError:
            pass

    def _reap_children(self):
        # reap and detect which worker died
        while True:
            try:
                pid, status = os.waitpid(-1, os.WNOHANG)
            except ChildProcessError:
                break
            if pid == 0:
                break
            # find which index
            idx = None
            for i, p in list(self.procs.items()):
                if p.pid == pid:
                    idx = i
                    break
            code = None
            if os.WIFEXITED(status):
                code = os.WEXITSTATUS(status)
            elif os.WIFSIGNALED(status):
                code = -os.WTERMSIG(status)
            print(f"[supervisor] child pid={pid} (worker {idx}) exited with {code}")
            if idx is not None:
                # remove from dict
                self.procs.pop(idx, None)
                # if not terminating, attempt restart
                if not self.terminate:
                    self._start_worker(idx)
        self.sigchld_flag = False

    def _broadcast_signal(self, sig):
        for i, p in list(self.procs.items()):
            try:
                os.kill(p.pid, sig)
            except ProcessLookupError:
                pass

    def _graceful_shutdown(self):
        print("[supervisor] graceful shutdown: telling workers to stop")
        # first ask nicely
        self._broadcast_signal(signal.SIGTERM)
        deadline = time.time() + GRACE_TIMEOUT
        while time.time() < deadline and any(p.poll() is None for p in self.procs.values()):
            time.sleep(0.1)
        # force kill remaining
        for i, p in list(self.procs.items()):
            if p.poll() is None:
                try:
                    print(f"[supervisor] worker {i} pid={p.pid} did not exit in time -> SIGKILL")
                    os.kill(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        # reap any left
        try:
            while True:
                pid, _ = os.waitpid(-1, os.WNOHANG)
                if pid <= 0:
                    break
        except ChildProcessError:
            pass
        print("[supervisor] shutdown complete")

    def _reload_config_and_restart_workers(self):
        print("[supervisor] reload: reading config")
        self.config = self._load_config()
        # soft restart workers one-by-one
        for idx in range(self.workers_count):
            p = self.procs.get(idx)
            if not p:
                # if it's missing, start it
                self._start_worker(idx)
                continue
            print(f"[supervisor] restarting worker {idx} pid={p.pid}")
            self._stop_worker(idx, signal.SIGTERM)
            # wait for exit
            deadline = time.time() + GRACE_TIMEOUT
            while time.time() < deadline and p.poll() is None:
                time.sleep(0.05)
            if p.poll() is None:
                try:
                    os.kill(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            # ensure reaped; start new
            self._start_worker(idx)
        print("[supervisor] reload complete")

    def _main_loop(self):
        while True:
            if self.sigchld_flag:
                self._reap_children()

            # poll subprocesses for unexpected death (fallback)
            for idx, p in list(self.procs.items()):
                if p.poll() is not None:
                    # process died without SIGCHLD (rare)
                    print(f"[supervisor] detected dead worker {idx} pid={p.pid}")
                    self.procs.pop(idx, None)
                    if not self.terminate:
                        self._start_worker(idx)

            if self.switch_mode:
                # translate into SIGUSR1/SIGUSR2 to workers
                sig = signal.SIGUSR1 if self.switch_mode == "light" else signal.SIGUSR2
                print(f"[supervisor] broadcasting {self.switch_mode} mode -> sending {sig}")
                self._broadcast_signal(sig)
                self.switch_mode = None

            if self.reload_requested:
                self._reload_config_and_restart_workers()
                self.reload_requested = False

            if self.terminate:
                self._graceful_shutdown()
                break

            time.sleep(0.2)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="config.json")
    ap.add_argument("--workers", type=int, default=3)
    args = ap.parse_args()
    s = Supervisor(args.config, args.workers)
    s.start()

