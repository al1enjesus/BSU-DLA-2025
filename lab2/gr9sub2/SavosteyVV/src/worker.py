import os
import sys
import time
import signal
import logging

logger = logging.getLogger("Worker")

CURRENT_MODE = "heavy"
IS_RUNNING = True

def _signal_handler(signum, frame):
    """Обработчик сигналов внутри процесса воркера"""
    global CURRENT_MODE, IS_RUNNING
    
    if signum == signal.SIGUSR1:
        CURRENT_MODE = "light"
        logging.info(f"Worker {os.getpid()}: Режим LIGHT")
    elif signum == signal.SIGUSR2:
        CURRENT_MODE = "heavy"
        logging.info(f"Worker {os.getpid()}: Режим HEAVY")
    elif signum == signal.SIGTERM:
        logger.info(f"Worker {os.getpid()}: Получен SIGTERM, выход...")
        IS_RUNNING = False

def _heavy_computation(duration):
    """Имитация нагрузки CPU (busy loop)"""
    end = time.time() + duration
    while time.time() < end:
        _ = 2134 * 5678  # Бессмысленные вычисления


def apply_scheduling(config, worker_idx):
    """Применяет настройки планирования из конфига"""
    scheduling_rules = config.get("scheduling", [])
    
    my_rule = next((item for item in scheduling_rules if item["worker_id"] == worker_idx), None)
    
    if not my_rule:
        logger.info(f"Worker {worker_idx}: Нет специфичных правил планирования.")
        return

    try:
        if "cpu_affinity" in my_rule:
            cpus = my_rule["cpu_affinity"]
            available_cpus = os.sched_getaffinity(0)
            valid_cpus = [c for c in cpus if c in available_cpus]
            
            if valid_cpus:
                os.sched_setaffinity(0, valid_cpus)
                logger.info(f"Worker {worker_idx}: Привязан к CPU {valid_cpus}")
            else:
                logger.warning(f"Worker {worker_idx}: Запрошенные CPU {cpus} недоступны! (Доступно: {available_cpus})")

        if "nice" in my_rule:
            nice_val = my_rule["nice"]
            new_nice = os.nice(nice_val)
            logger.info(f"Worker {worker_idx}: Nice изменен на {new_nice} (запрошено {nice_val})")

    except PermissionError:
        logger.error(f"Worker {worker_idx}: Ошибка прав доступа! (Для nice < 0 нужен sudo)")
    except Exception as e:
        logger.error(f"Worker {worker_idx}: Ошибка настройки планирования: {e}")


def run_worker(config, worker_idx):
    """
    Точка входа в логику воркера.
    Этот код выполняется уже внутри дочернего процесса.
    """
    # 1. Смена имени процесса (для удобства в top/ps)
    try:
        import setproctitle
        setproctitle.setproctitle(f"lab2-worker-{worker_idx}")
    except ImportError:
        pass
    mode_params = config.get(f"mode_{CURRENT_MODE}", {"work_sec": 0.1, "sleep_sec": 0.1})

    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGTERM, _signal_handler)
    signal.signal(signal.SIGUSR1, _signal_handler)
    signal.signal(signal.SIGUSR2, _signal_handler)
    apply_scheduling(config, worker_idx)
    logger.info(f"Worker запущен. PID: {os.getpid()}")

    while IS_RUNNING:
        work_t = mode_params["work_sec"]
        sleep_t = mode_params["sleep_sec"]
        _heavy_computation(work_t)
        time.sleep(sleep_t)

        
    sys.exit(0)