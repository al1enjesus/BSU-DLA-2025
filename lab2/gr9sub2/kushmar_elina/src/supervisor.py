#!/usr/bin/env python3
"""
Supervisor process for Linux processes lab (Lab 2)
Manages worker processes and handles signals
"""

import os
import sys
import time
import signal
import logging
import subprocess
import argparse
import json
from typing import Dict, List, Any
from dataclasses import dataclass
from datetime import datetime, timedelta

@dataclass
class WorkerInfo:
    """Information about a worker process"""
    pid: int
    start_time: datetime
    restart_count: int = 0
    last_restart_time: datetime = None

class Supervisor:
    def __init__(self, config_file: str):
        self.config_file = config_file
        self.config = self.load_config()
        self.workers: Dict[int, WorkerInfo] = {}
        self.running = True
        self.worker_script = "worker.py"  # Path to worker script
        
        # Restart rate limiting
        self.restart_history = []
        self.max_restarts = 5
        self.restart_window = 30  # seconds
        
        # Setup signal handlers
        self.setup_signal_handlers()
        
        # Setup logging
        self.setup_logging()

    def load_config(self) -> Dict[str, Any]:
        """Load configuration from file"""
        try:
            with open(self.config_file, 'r') as f:
                if self.config_file.endswith('.json'):
                    return json.load(f)
                else:
                    # Simple KEY=VALUE format
                    config = {}
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith('#'):
                            key, value = line.split('=', 1)
                            config[key.strip()] = value.strip()
                    return config
        except Exception as e:
            logging.error(f"Failed to load config: {e}")
            return {}

    def setup_logging(self):
        """Setup logging format"""
        logging.basicConfig(
            level=logging.INFO,
            format='SUPERVISOR[%(process)d] %(asctime)s - %(levelname)s - %(message)s',
            datefmt='%H:%M:%S'
        )
        self.logger = logging.getLogger()

    def setup_signal_handlers(self):
        """Setup signal handlers"""
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGHUP, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)
        signal.signal(signal.SIGCHLD, self.signal_handler)

    def signal_handler(self, signum, frame):
        """Handle incoming signals"""
        if signum in (signal.SIGTERM, signal.SIGINT):
            self.logger.info(f"Received signal {signum}, initiating graceful shutdown")
            self.graceful_shutdown()
            
        elif signum == signal.SIGHUP:
            self.logger.info("Received SIGHUP, reloading configuration")
            self.graceful_reload()
            
        elif signum == signal.SIGUSR1:
            self.logger.info("Received SIGUSR1, switching workers to light mode")
            self.broadcast_signal(signal.SIGUSR1)
            
        elif signum == signal.SIGUSR2:
            self.logger.info("Received SIGUSR2, switching workers to heavy mode")
            self.broadcast_signal(signal.SIGUSR2)
            
        elif signum == signal.SIGCHLD:
            self.handle_child_signal()

    def can_restart(self) -> bool:
        """Check if we can restart a worker (rate limiting)"""
        now = datetime.now()
        # Remove old restarts from history
        self.restart_history = [t for t in self.restart_history 
                               if now - t < timedelta(seconds=self.restart_window)]
        
        # Check if we've exceeded the rate limit
        return len(self.restart_history) < self.max_restarts

    def start_worker(self) -> int:
        """Start a new worker process"""
        try:
            # Build command for worker
            cmd = [
                sys.executable, self.worker_script,
                f"--heavy-work-us={self.config.get('mode_heavy', {}).get('work_us', 9000)}",
                f"--heavy-sleep-us={self.config.get('mode_heavy', {}).get('sleep_us', 1000)}",
                f"--light-work-us={self.config.get('mode_light', {}).get('work_us', 2000)}",
                f"--light-sleep-us={self.config.get('mode_light', {}).get('sleep_us', 8000)}"
            ]
            
            # Start worker process
            process = subprocess.Popen(cmd)
            pid = process.pid
            
            # Record worker info
            self.workers[pid] = WorkerInfo(
                pid=pid,
                start_time=datetime.now()
            )
            
            self.logger.info(f"Started worker with PID {pid}")
            return pid
            
        except Exception as e:
            self.logger.error(f"Failed to start worker: {e}")
            return -1

    def stop_worker(self, pid: int, timeout: int = 5):
        """Stop a worker process gracefully"""
        try:
            if pid in self.workers:
                self.logger.info(f"Stopping worker {pid}")
                os.kill(pid, signal.SIGTERM)
                
                # Wait for graceful termination
                start_time = time.time()
                while time.time() - start_time < timeout:
                    try:
                        os.kill(pid, 0)  # Check if process exists
                        time.sleep(0.1)
                    except OSError:
                        # Process has terminated
                        if pid in self.workers:
                            del self.workers[pid]
                        self.logger.info(f"Worker {pid} terminated gracefully")
                        return
                
                # Force kill if timeout exceeded
                self.logger.warning(f"Force killing worker {pid}")
                os.kill(pid, signal.SIGKILL)
                if pid in self.workers:
                    del self.workers[pid]
                    
        except OSError as e:
            self.logger.error(f"Error stopping worker {pid}: {e}")

    def broadcast_signal(self, sig: int):
        """Send signal to all workers"""
        for pid in list(self.workers.keys()):
            try:
                os.kill(pid, sig)
                self.logger.info(f"Sent signal {sig} to worker {pid}")
            except OSError as e:
                self.logger.error(f"Failed to send signal to worker {pid}: {e}")

    def handle_child_signal(self):
        """Handle SIGCHLD - worker process termination"""
        try:
            while True:
                # Wait for any child process without blocking
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:  # No more children to wait for
                    break
                
                if pid in self.workers:
                    worker_info = self.workers[pid]
                    
                    # Check exit status
                    if os.WIFEXITED(status):
                        exit_code = os.WEXITSTATUS(status)
                        self.logger.info(f"Worker {pid} exited with code {exit_code}")
                    elif os.WIFSIGNALED(status):
                        sig = os.WTERMSIG(status)
                        self.logger.info(f"Worker {pid} killed by signal {sig}")
                    
                    # Remove from active workers
                    del self.workers[pid]
                    
                    # Check if we should restart
                    if self.running and self.can_restart():
                        self.logger.info(f"Restarting worker {pid}")
                        self.restart_history.append(datetime.now())
                        self.start_worker()
                    else:
                        self.logger.warning(f"Not restarting worker {pid} (rate limit exceeded)")
                        
        except ChildProcessError:
            # No child processes
            pass

    def graceful_shutdown(self):
        """Graceful shutdown of all workers"""
        self.running = False
        self.logger.info("Starting graceful shutdown")
        
        # Send SIGTERM to all workers
        self.broadcast_signal(signal.SIGTERM)
        
        # Wait for workers to terminate
        timeout = 5  # seconds
        start_time = time.time()
        
        while self.workers and (time.time() - start_time < timeout):
            time.sleep(0.1)
        
        # Force kill any remaining workers
        for pid in list(self.workers.keys()):
            self.logger.warning(f"Force killing worker {pid}")
            os.kill(pid, signal.SIGKILL)
        
        self.logger.info("Supervisor shutdown complete")
        sys.exit(0)

    def graceful_reload(self):
        """Graceful reload - restart workers with new config"""
        self.logger.info("Starting graceful reload")
        
        # Reload configuration
        old_config = self.config
        self.config = self.load_config()
        
        # Stop all current workers gracefully
        for pid in list(self.workers.keys()):
            self.stop_worker(pid)
        
        # Start new workers
        worker_count = int(self.config.get('workers', 3))
        for _ in range(worker_count):
            self.start_worker()
        
        self.logger.info("Graceful reload completed")

    def run(self):
        """Main supervisor loop"""
        self.logger.info(f"Supervisor started with PID {os.getpid()}")
        self.logger.info(f"Configuration: {self.config}")
        
        # Start initial workers
        worker_count = int(self.config.get('workers', 3))
        for i in range(worker_count):
            self.start_worker()
        
        self.logger.info(f"Started {worker_count} workers")
        
        # Main loop
        try:
            while self.running:
                # Check worker status periodically
                current_workers = len(self.workers)
                if current_workers < worker_count and self.can_restart():
                    self.logger.info(f"Only {current_workers} workers running, starting additional worker")
                    self.start_worker()
                
                time.sleep(1)
                
        except KeyboardInterrupt:
            self.logger.info("Keyboard interrupt received")
            self.graceful_shutdown()
        except Exception as e:
            self.logger.error(f"Unexpected error: {e}")
            self.graceful_shutdown()

def main():
    """Main entry point for supervisor"""
    parser = argparse.ArgumentParser(description='Supervisor process')
    parser.add_argument('--config', '-c', required=True,
                       help='Configuration file path')
    parser.add_argument('--worker-script', '-w', default='worker.py',
                       help='Path to worker script')
    
    args = parser.parse_args()
    
    try:
        supervisor = Supervisor(args.config)
        supervisor.worker_script = args.worker_script
        supervisor.run()
    except Exception as e:
        logging.error(f"Failed to start supervisor: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()