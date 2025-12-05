#!/usr/bin/env python3
import os
import sys
import time
import signal
import logging
from collections import deque

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from config_loader import load_config
from worker import run_worker

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger("Supervisor")

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "config.json")
WORKER_COUNTER = 0
WORKERS = set()   
CONFIG = {}
SHUTDOWN_FLAG = False
RESTART_HISTORY = deque()

# --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

def check_restart_limits():
    """Rate Limiter: возвращает True, если можно рестартовать"""
    now = time.time()
    window = CONFIG.get("restart_window_sec", 30)
    limit = CONFIG.get("max_restarts", 5)

    # Чистим старые записи
    while RESTART_HISTORY and now - RESTART_HISTORY[0] > window:
        RESTART_HISTORY.popleft()

    if len(RESTART_HISTORY) >= limit:
        return False
    
    RESTART_HISTORY.append(now)
    return True

def spawn_worker():
    """Порождает процесс (fork) и запускает в нем логику воркера"""
    global WORKER_COUNTER
    try:
        pid = os.fork()
    except OSError as e:
        logger.error(f"Fork failed: {e}")
        return

    worker_idx = WORKER_COUNTER 
    
    if pid == 0:
        # === ДОЧЕРНИЙ ПРОЦЕСС ===
        signal.signal(signal.SIGCHLD, signal.SIG_DFL)
        signal.signal(signal.SIGHUP, signal.SIG_DFL)
        
        run_worker(CONFIG, worker_idx) 
        
    else:
        # === РОДИТЕЛЬСКИЙ ПРОЦЕСС ===
        WORKERS.add(pid)
        WORKER_COUNTER += 1 # Увеличиваем счетчик
        logger.info(f"Заспавнил воркера {pid} (Index: {worker_idx})")

def stop_all_workers():
    """Посылает SIGTERM всем и ждет завершения"""
    if not WORKERS:
        return

    logger.info("Отправка SIGTERM всем воркерам...")
    for pid in list(WORKERS):
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass 

    # Graceful wait (до 5 сек)
    deadline = time.time() + 5
    while WORKERS and time.time() < deadline:
        try:
            time.sleep(0.1)
        except KeyboardInterrupt:
            break
            
    # Force kill
    if WORKERS:
        logger.warning(f"Остались висеть: {WORKERS}. Посылаю SIGKILL.")
        for pid in list(WORKERS):
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError: 
                pass

# --- ОБРАБОТЧИКИ СИГНАЛОВ СУПЕРВИЗОРА ---

def handle_shutdown(signum, frame):
    """SIGTERM / SIGINT"""
    global SHUTDOWN_FLAG
    logger.info("Получен сигнал остановки. Завершение работы...")
    SHUTDOWN_FLAG = True
    stop_all_workers()
    sys.exit(0)

def handle_reload(signum, frame):
    """SIGHUP: Перечитывает конфиг и перезапускает всех"""
    global CONFIG
    logger.info("Получен SIGHUP. Перезагрузка конфигурации...")
    
    new_config = load_config(CONFIG_PATH)
    CONFIG = new_config
    
    stop_all_workers()
    
    target = CONFIG.get("workers_count", 2)
    logger.info(f"Запуск {target} новых воркеров...")
    for _ in range(target):
        spawn_worker()

def handle_sigchld(signum, frame):
    """
    SIGCHLD: Основной механизм 'подбора' зомби и рестарта.
    Вызывается, когда любой дочерний процесс меняет состояние.
    """
    while True:
        try:
            pid, status = os.waitpid(-1, os.WNOHANG)
            if pid <= 0:
                break # Больше нет кандидатов на подбор
            
            # Если мы тут, значит pid реально завершился
            if pid in WORKERS:
                WORKERS.remove(pid)
                code = os.waitstatus_to_exitcode(status) if hasattr(os, 'waitstatus_to_exitcode') else status >> 8
                logger.warning(f"Воркер {pid} умер (код {code}).")
                
                # Логика перезапуска:
                # Перезапускаем только если НЕ идет процедура выключения
                if not SHUTDOWN_FLAG:
                    if check_restart_limits():
                        logger.info("Перезапускаю взамен упавшего...")
                        spawn_worker()
                    else:
                        logger.critical("ПРЕВЫШЕН ЛИМИТ РЕСТАРТОВ. Воркер не будет восстановлен.")
                        
        except ChildProcessError:
            break
        except OSError:
            break

def handle_broadcast(signum, frame):
    """SIGUSR1 / SIGUSR2: просто транслируем детям"""
    sig_name = "SIGUSR1" if signum == signal.SIGUSR1 else "SIGUSR2"
    logger.info(f"Broadcasting {sig_name} to workers...")
    for pid in list(WORKERS):
        try:
            os.kill(pid, signum)
        except OSError:
            pass

# --- MAIN ---

def main():
    global CONFIG
    
    try:
        import setproctitle
        setproctitle.setproctitle("lab2-supervisor")
    except ImportError:
        pass

    CONFIG = load_config(CONFIG_PATH)
    
    signal.signal(signal.SIGTERM, handle_shutdown) 
    signal.signal(signal.SIGINT, handle_shutdown) 
    signal.signal(signal.SIGHUP, handle_reload) 
    signal.signal(signal.SIGUSR1, handle_broadcast)
    signal.signal(signal.SIGUSR2, handle_broadcast)
    signal.signal(signal.SIGCHLD, handle_sigchld)
    
    logger.info(f"Супервизор инициализирован. PID: {os.getpid()}")
    
    count = CONFIG.get("workers_count", 2)
    for _ in range(count):
        spawn_worker()
        
    while True:
        try:
            signal.pause()
        except KeyboardInterrupt:
            handle_shutdown(None, None)

if __name__ == "__main__":
    main()