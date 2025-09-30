import os
import time
import signal
import json
from .logger import Logger
from multiprocessing import shared_memory

def busy_wait_us(us: int):
    time.sleep(us / 1_000_000)

def _apply_assignment(nice_val, cpus, logger, comp, pid):
    # применить nice
    if nice_val is not None:
        try:
            # setpriority может вызывать исключения, поэтому нужно сделать обработку и fallback по возможности
            try:
                os.setpriority(os.PRIO_PROCESS, 0, int(nice_val))
            except AttributeError:
                # fallback: инкремент значения nice на разницу delta
                cur = os.getpriority(os.PRIO_PROCESS, 0)
                delta = int(nice_val) - cur
                if delta != 0:
                    os.nice(delta)
            logger.info(comp, 'applied nice', pid=pid, nice=nice_val)
        except Exception as e:
            logger.error(comp, 'failed apply nice', pid=pid, error=str(e))

    # применение cpu affinity
    if cpus is not None and hasattr(os, 'sched_setaffinity'):
        try:
            cpuset = set(int(x) for x in cpus)
            os.sched_setaffinity(0, cpuset)
            logger.info(comp, 'applied affinity', pid=pid, cpus=list(cpuset))
        except Exception as e:
            logger.error(comp, 'failed apply affinity', pid=pid, error=str(e))

def run_worker(worker_id: int, modes: dict, log_path: str, initial_mode: str, nice: int=None, cpus=None, shm_name: str=None):
    logger = Logger(log_path)
    comp = f'worker-{worker_id}'
    pid = os.getpid()

    # первоначальное применение значений
    _apply_assignment(nice, cpus, logger, comp, pid)

    # connect to shared memory if provided
    shm = None
    if shm_name:
        try:
            shm = shared_memory.SharedMemory(name=shm_name)
            logger.info(comp, 'connected to shared memory', shm_name=shm_name)
        except Exception as e:
            shm = None
            logger.error(comp, 'failed connect shm', error=str(e))

    terminate = False
    mode = initial_mode or 'heavy'

    def _term(sig, frame):
        nonlocal terminate
        terminate = True
        logger.info(comp, 'sigterm received', pid=pid, sig=sig)

    def _usr1(sig, frame):
        nonlocal mode
        mode = 'light'
        logger.info(comp, 'sigusr1 -> light', pid=pid)

    def _usr2(sig, frame):
        nonlocal mode
        mode = 'heavy'
        logger.info(comp, 'sigusr2 -> heavy', pid=pid)

    def _hup(sig, frame):
        # чтение переменных из конфига в shared-memory
        nonlocal shm
        if not shm:
            logger.error(comp, 'sighup but no shared memory connected', pid=pid)
            return
        try:
            buf = bytes(shm.buf)
            try:
                end = buf.index(0)
                data = buf[:end].decode('utf-8')
            except ValueError:
                data = buf.decode('utf-8')
            doc = json.loads(data)
            assigns = doc.get('assignments', [])
            if worker_id < len(assigns):
                a = assigns[worker_id]
                _apply_assignment(a.get('nice'), a.get('cpus'), logger, comp, pid)
                logger.info(comp, 'hup applied assignment', pid=pid, assignment=a)
            else:
                logger.info(comp, 'hup but no assignment for this worker id', pid=pid)
        except Exception as e:
            logger.error(comp, 'failed to apply assignment from shm on hup', pid=pid, error=str(e))

    signal.signal(signal.SIGTERM, _term)
    signal.signal(signal.SIGINT, _term)
    signal.signal(signal.SIGUSR1, _usr1)
    signal.signal(signal.SIGUSR2, _usr2)
    signal.signal(signal.SIGHUP, _hup)

    logger.info(comp, 'started', pid=pid, mode=mode)

    tick = 0
    last_stat = time.time()
    while not terminate:
        tick += 1
        m = modes.get('mode_heavy') if mode == 'heavy' else modes.get('mode_light')
        work_us = int(m.get('work_us', 1000))
        sleep_us = int(m.get('sleep_us', 1000))
        busy_wait_us(work_us)
        remaining = sleep_us / 1_000_000.0
        while remaining > 0 and not terminate:
            s = min(0.1, remaining)
            time.sleep(s)
            remaining -= s
        if time.time() - last_stat >= 1.0:
            try:
                aff = list(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None
            except Exception:
                aff = None
            try:
                cur_nice = os.getpriority(os.PRIO_PROCESS, 0)
            except Exception:
                cur_nice = None
            logger.info(comp, 'tick', pid=pid, tick=tick, mode=mode, nice=cur_nice, affinity=aff)
            last_stat = time.time()

    logger.info(comp, 'exiting', pid=pid, ticks=tick)
    try:
        if shm:
            shm.close()
    except Exception:
        pass
    logger.close()
