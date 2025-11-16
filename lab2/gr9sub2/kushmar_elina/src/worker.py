#!/usr/bin/env python3
"""
Worker process for Linux processes lab (Lab 2)
Handles signals and switches between heavy/light workload modes
"""

import os
import sys
import time
import signal
import logging
import argparse
from typing import Dict, Any

class Worker:
    def __init__(self, config: Dict[str, Any]):
        self.pid = os.getpid()
        self.running = True
        self.mode = "heavy"  # Current mode: "heavy" or "light"
        
        # Load configuration
        self.config = config
        self.work_us = config.get('mode_heavy', {}).get('work_us', 9000)
        self.sleep_us = config.get('mode_heavy', {}).get('sleep_us', 1000)
        
        # Statistics
        self.iteration = 0
        self.total_work_time = 0
        self.total_sleep_time = 0
        
        # Setup signal handlers
        self.setup_signal_handlers()
        
        # Setup logging
        self.setup_logging()

    def setup_logging(self):
        """Setup logging format"""
        logging.basicConfig(
            level=logging.INFO,
            format=f'WORKER[{self.pid}] %(asctime)s - %(levelname)s - %(message)s',
            datefmt='%H:%M:%S'
        )
        self.logger = logging.getLogger()

    def setup_signal_handlers(self):
        """Setup signal handlers for graceful shutdown and mode switching"""
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGUSR1, self.signal_handler)
        signal.signal(signal.SIGUSR2, self.signal_handler)

    def signal_handler(self, signum, frame):
        """Handle incoming signals"""
        if signum == signal.SIGTERM or signum == signal.SIGINT:
            self.logger.info(f"Received signal {signum}, initiating graceful shutdown")
            self.running = False
            
        elif signum == signal.SIGUSR1:
            self.switch_to_light_mode()
            
        elif signum == signal.SIGUSR2:
            self.switch_to_heavy_mode()

    def switch_to_light_mode(self):
        """Switch to light workload mode"""
        if self.mode != "light":
            self.mode = "light"
            self.work_us = self.config.get('mode_light', {}).get('work_us', 2000)
            self.sleep_us = self.config.get('mode_light', {}).get('sleep_us', 8000)
            self.logger.info(f"Switched to LIGHT mode (work: {self.work_us}µs, sleep: {self.sleep_us}µs)")

    def switch_to_heavy_mode(self):
        """Switch to heavy workload mode"""
        if self.mode != "heavy":
            self.mode = "heavy"
            self.work_us = self.config.get('mode_heavy', {}).get('work_us', 9000)
            self.sleep_us = self.config.get('mode_heavy', {}).get('sleep_us', 1000)
            self.logger.info(f"Switched to HEAVY mode (work: {self.work_us}µs, sleep: {self.sleep_us}µs)")

    def simulate_work(self, duration_us: int):
        """Simulate CPU work by busy waiting"""
        start_time = time.time()
        end_time = start_time + (duration_us / 1_000_000)
        
        # Simple busy wait to simulate CPU work
        while time.time() < end_time:
            # Some computational work
            _ = sum(i*i for i in range(1000))
            
        actual_duration = (time.time() - start_time) * 1_000_000
        return actual_duration

    def get_cpu_affinity_info(self) -> str:
        """Get CPU affinity information (placeholder for macOS/Linux compatibility)"""
        try:
            # On Linux, we could use os.sched_getaffinity()
            # On macOS, this isn't available, so we return a placeholder
            return "CPU affinity: N/A (macOS)"
        except AttributeError:
            return "CPU affinity: Not available on this system"

    def run(self):
        """Main worker loop"""
        self.logger.info(f"Worker started with PID {self.pid}")
        self.logger.info(f"Initial mode: {self.mode.upper()} (work: {self.work_us}µs, sleep: {self.sleep_us}µs)")
        self.logger.info(self.get_cpu_affinity_info())
        
        try:
            while self.running:
                self.iteration += 1
                
                # Work phase
                work_start = time.time()
                actual_work_us = self.simulate_work(self.work_us)
                work_end = time.time()
                
                # Sleep phase
                sleep_start = time.time()
                time.sleep(self.sleep_us / 1_000_000)
                sleep_end = time.time()
                
                # Calculate actual durations
                actual_work_duration = (work_end - work_start) * 1_000_000
                actual_sleep_duration = (sleep_end - sleep_start) * 1_000_000
                
                self.total_work_time += actual_work_duration
                self.total_sleep_time += actual_sleep_duration
                
                # Log every 10 iterations to avoid spam
                if self.iteration % 10 == 0:
                    avg_work = self.total_work_time / self.iteration
                    avg_sleep = self.total_sleep_time / self.iteration
                    
                    self.logger.info(
                        f"Iteration {self.iteration}: "
                        f"work={actual_work_duration:.0f}µs, "
                        f"sleep={actual_sleep_duration:.0f}µs, "
                        f"avg_work={avg_work:.0f}µs, "
                        f"avg_sleep={avg_sleep:.0f}µs, "
                        f"mode={self.mode}"
                    )
                    
        except KeyboardInterrupt:
            self.logger.info("Keyboard interrupt received")
        except Exception as e:
            self.logger.error(f"Unexpected error: {e}")
        finally:
            self.shutdown()

    def shutdown(self):
        """Graceful shutdown procedure"""
        self.logger.info("Shutting down worker")
        
        # Print final statistics
        if self.iteration > 0:
            self.logger.info(
                f"Final stats: {self.iteration} iterations, "
                f"total work: {self.total_work_time/1000:.2f}ms, "
                f"total sleep: {self.total_sleep_time/1000:.2f}ms"
            )
        
        self.logger.info("Worker shutdown complete")
        sys.exit(0)


def load_config_from_args() -> Dict[str, Any]:
    """Load configuration from command line arguments"""
    parser = argparse.ArgumentParser(description='Worker process')
    
    # Mode configurations
    parser.add_argument('--heavy-work-us', type=int, default=9000, 
                       help='Work duration in microseconds for heavy mode')
    parser.add_argument('--heavy-sleep-us', type=int, default=1000,
                       help='Sleep duration in microseconds for heavy mode')
    parser.add_argument('--light-work-us', type=int, default=2000,
                       help='Work duration in microseconds for light mode')
    parser.add_argument('--light-sleep-us', type=int, default=8000,
                       help='Sleep duration in microseconds for light mode')
    
    args = parser.parse_args()
    
    config = {
        'mode_heavy': {
            'work_us': args.heavy_work_us,
            'sleep_us': args.heavy_sleep_us
        },
        'mode_light': {
            'work_us': args.light_work_us,
            'sleep_us': args.light_sleep_us
        }
    }
    
    return config


def main():
    """Main entry point for the worker"""
    try:
        config = load_config_from_args()
        worker = Worker(config)
        worker.run()
    except Exception as e:
        print(f"Failed to start worker: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()