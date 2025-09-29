# вставьте это в src/supervisor.py, заменяя класс Supervisor
import json
import time
import os
import signal
import argparse
from multiprocessing import Process
from multiprocessing import shared_memory
from .logger import Logger
from .worker import run_worker

SHM_SIZE = 64 * 1024  # 64 KiB for JSON assignment blob

class Supervisor:
    def __init__(self, config_path):
        self.config_path = config_path
        self.config = self._load_config()
        self.log = Logger(self.config.get('log_path', 'logs/demo.log'))
        # planner removed — using shared memory instead
        self.workers = {}  # slot -> {proc, pid, start, restarts}
        rl = self.config.get('restart_limit', {})
        self.restart_count = int(rl.get('count', 5))
        self.restart_window = int(rl.get('window_seconds', 30))
        self.need_reload = False
        self.terminated = False

        # create shared memory segment
        # name chosen deterministically per supervisor pid so tests/other sups don't collide
        self.shm_name = f"sup_shm_{os.getpid()}"
        try:
            # try to unlink existing (leftover) then create
            try:
                existing = shared_memory.SharedMemory(name=self.shm_name)
                existing.close()
                existing.unlink()
            except FileNotFoundError:
                pass
            self.shm = shared_memory.SharedMemory(create=True, size=SHM_SIZE, name=self.shm_name)
            # initialize with assignments from config
            self._write_shm_assignments(self._build_assignments_from_config(self.config))
            self.log.info('supervisor', 'created shared memory', shm_name=self.shm_name, size=SHM_SIZE)
        except Exception as e:
            self.log.error('supervisor', 'failed to create shared memory', error=str(e))
            raise

        signal.signal(signal.SIGCHLD, self._sigchld)
        signal.signal(signal.SIGTERM, self._sigterm)
        signal.signal(signal.SIGINT, self._sigterm)
        signal.signal(signal.SIGHUP, self._sighup)
        signal.signal(signal.SIGUSR1, self._sigusr1)
        signal.signal(signal.SIGUSR2, self._sigusr2)

    def _build_assignments_from_config(self, cfg):
        # Expect arrays 'nice_values' and 'cpu_affinity' in config; fallback sane defaults
        workers = int(cfg.get('workers', 2))
        nice_vals = cfg.get('nice_values', [])
        cpu_aff = cfg.get('cpu_affinity', [])
        assigns = []
        for i in range(workers):
            nice = None
            cpus = None
            if i < len(nice_vals):
                try:
                    nice = int(nice_vals[i])
                except Exception:
                    nice = None
            if i < len(cpu_aff):
                cpus = cpu_aff[i]
            assigns.append({'nice': nice, 'cpus': cpus})
        return assigns

    def _write_shm_assignments(self, assigns):
        """Write JSON to shared memory (null-terminated)."""
        try:
            b = json.dumps({'assignments': assigns}).encode('utf-8')
            if len(b) >= SHM_SIZE:
                raise ValueError("assignment blob too large for SHM")
            # zero buffer then write
            self.shm.buf[:] = b'\x00' * SHM_SIZE
            self.shm.buf[:len(b)] = b
            # optional: null-terminate already done by zeros
            self.log.info('supervisor', 'wrote assignments to shm', shm_name=self.shm_name, count=len(assigns))
        except Exception as e:
            self.log.error('supervisor', 'failed write shm', error=str(e))

    # ---- public interface ----
    def start_workers(self, n=None):
        n = n or int(self.config.get('workers', 2))
        for i in range(n):
            self._spawn(i)

    def reload_config(self):
        self._reload_config()

    def stop_all(self):
        self.terminated = True
        self._graceful_shutdown()

    def broadcast_light(self):
        self._sigusr1(None, None)

    def broadcast_heavy(self):
        self._sigusr2(None, None)

    def reap_children(self):
        self._reap_children()

    def _load_config(self):
        with open(self.config_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    def _sigchld(self, signum, frame):
        self.log.info('supervisor', 'sigchld received', sig=signum)

    def _sigterm(self, signum, frame):
        self.log.info('supervisor', 'termination signal received', sig=signum)
        self.terminated = True

    def _sighup(self, signum, frame):
        self.log.info('supervisor', 'sighup reload requested', sig=signum)
        self.need_reload = True

    def _sigusr1(self, signum, frame):
        self.log.info('supervisor', 'broadcast sigusr1 -> light')
        for idx, w in list(self.workers.items()):
            try:
                os.kill(w['pid'], signal.SIGUSR1)
            except Exception:
                pass

    def _sigusr2(self, signum, frame):
        self.log.info('supervisor', 'broadcast sigusr2 -> heavy')
        for idx, w in list(self.workers.items()):
            try:
                os.kill(w['pid'], signal.SIGUSR2)
            except Exception:
                pass

    def start(self):
        n = int(self.config.get('workers', 2))
        self.log.info('supervisor', 'starting', workers=n)
        for i in range(n):
            self._spawn(i)
        while True:
            if self.terminated:
                self._graceful_shutdown()
                break
            if self.need_reload:
                self._reload_config()
                self.need_reload = False
            self._reap_children()
            time.sleep(0.5)

    def _spawn(self, idx):
        # read assignment for slot idx from current config (we store in self.config too)
        assigns = self._build_assignments_from_config(self.config)
        assign = assigns[idx] if idx < len(assigns) else {'nice': None, 'cpus': None}
        try:
            p = Process(target=run_worker, args=(
                idx,
                {'mode_heavy': self.config.get('mode_heavy', {}), 'mode_light': self.config.get('mode_light', {})},
                self.config.get('log_path', 'logs/demo.log'),
                'heavy',
                assign.get('nice'),
                assign.get('cpus'),
                self.shm_name  # new arg: shared memory name
            ))
            p.start()
            pid = p.pid
            old = self.workers.get(idx, {})
            self.workers[idx] = {'proc': p, 'pid': pid, 'start': time.time(), 'restarts': old.get('restarts', [])}
            self.log.info('supervisor', 'spawned worker', idx=idx, pid=pid, nice=assign.get('nice'), cpus=assign.get('cpus'))
        except Exception as e:
            self.log.error('supervisor', 'failed to spawn worker', idx=idx, error=str(e))

    def _reap_children(self):
        import errno
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    break
                idx = None
                for k, v in list(self.workers.items()):
                    if v.get('pid') == pid:
                        idx = k
                        break
                self.log.info('supervisor', 'child exited', pid=pid, status=status, slot=idx)
                if idx is not None:
                    try:
                        self.workers[idx]['proc'].join(timeout=0)
                    except Exception:
                        pass
                    now = time.time()
                    restarts = self.workers[idx].get('restarts', [])
                    restarts = [t for t in restarts if now - t <= self.restart_window]
                    if len(restarts) >= self.restart_count:
                        self.log.error('supervisor', 'restart limit reached, not restarting', idx=idx, pid=pid)
                        del self.workers[idx]
                    else:
                        restarts.append(now)
                        self.workers[idx]['restarts'] = restarts
                        self.log.info('supervisor', 'restarting worker', idx=idx)
                        self._spawn(idx)
                else:
                    self.log.info('supervisor', 'unknown child exited', pid=pid)
        except ChildProcessError:
            pass
        except OSError as e:
            if e.errno != errno.ECHILD:
                raise

    def _reload_config(self):
        """Reload configuration, update shared memory assignments and signal workers to apply them"""
        self.log.info('supervisor', 'reloading config')
        newc = self._load_config()
        self.config = newc

        if self.terminated:
            self.log.info('supervisor', 'terminated flag set, not adjusting workers')
            return

        desired = int(self.config.get('workers', 2))
        current = len(self.workers)

        # scale up
        if desired > current:
            self.log.info('supervisor', 'scaling up workers', from_n=current, to_n=desired)
            for i in range(current, desired):
                self._spawn(i)

        # scale down
        elif desired < current:
            self.log.info('supervisor', 'scaling down workers', from_n=current, to_n=desired)
            for idx in sorted(self.workers.keys(), reverse=True)[0:current - desired]:
                w = self.workers.get(idx)
                if not w:
                    continue
                try:
                    os.kill(w['pid'], signal.SIGTERM)
                    self.log.info('supervisor', 'sent sigterm to worker', idx=idx, pid=w['pid'])
                except Exception:
                    pass
                del self.workers[idx]

        # update shared memory with new assignments
        assigns = self._build_assignments_from_config(self.config)
        self._write_shm_assignments(assigns)

        # tell workers to reload from shared memory
        for idx, w in list(self.workers.items()):
            try:
                os.kill(w['pid'], signal.SIGHUP)
                self.log.info('supervisor', 'sent sighup to worker for config reload', idx=idx, pid=w['pid'])
            except Exception:
                pass

        self.log.info('supervisor', 'config reload complete')

    def _graceful_shutdown(self):
        self.log.info('supervisor', 'graceful shutdown initiated')
        for idx, w in list(self.workers.items()):
            try:
                os.kill(w['pid'], signal.SIGTERM)
            except Exception:
                pass
        deadline = time.time() + 5.0
        while time.time() < deadline and any(w['proc'].is_alive() for w in self.workers.values()):
            time.sleep(0.1)
        for idx, w in list(self.workers.items()):
            if w['proc'].is_alive():
                try:
                    os.kill(w['pid'], signal.SIGKILL)
                except Exception:
                    pass
        # cleanup shared memory
        try:
            self.shm.close()
            self.shm.unlink()
        except Exception:
            pass
        self.log.info('supervisor', 'shutdown complete')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--config', required=True)
    args = p.parse_args()
    sup = Supervisor(args.config)
    sup.start()