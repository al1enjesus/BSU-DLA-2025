#!/usr/bin/env python3
import os
import sys
import platform
from typing import List, Optional
import subprocess

class ProcessScheduler:
    """Класс для управления планированием процессов"""

    @staticmethod
    def set_nice(pid: int, nice_value: int) -> bool:
        """Установка значения nice для процесса"""
        try:
            if platform.system() == "Windows":
                print(f"Warning: nice not supported on Windows")
                return False

            if nice_value < 0 and os.geteuid() != 0:
                print(f"Warning: установка negative nice={nice_value} требует root")
                return False

            # Для текущего процесса используем os.nice напрямую
            if pid == os.getpid():
                os.nice(nice_value - os.nice(0))  # можно оставить, т.к. один вызов
                return True
            else:
                # Для других процессов используем renice
                result = subprocess.run(
                    ['renice', '-n', str(nice_value), '-p', str(pid)],
                    capture_output=True, text=True
                )
                if result.returncode != 0:
                    print(f"renice failed: {result.stderr.strip()}")
                return result.returncode == 0

        except Exception as e:
            print(f"Error setting nice for PID {pid}: {e}")
            return False

    @staticmethod
    def set_cpu_affinity(pid: int, cpus: List[int]) -> bool:
        """Установка CPU affinity для процесса"""
        try:
            if platform.system() != "Linux":
                print(f"Warning: CPU affinity only fully supported on Linux")
                return False

            # Проверка наличия taskset
            if subprocess.run(['which', 'taskset'], capture_output=True).returncode != 0:
                print("Warning: taskset not found, skipping CPU affinity")
                return False

            cpu_mask = ','.join(map(str, cpus))
            result = subprocess.run(
                ['taskset', '-cp', cpu_mask, str(pid)],
                capture_output=True, text=True
            )
            if result.returncode != 0:
                print(f"taskset failed: {result.stderr.strip()}")
            return result.returncode == 0

        except Exception as e:
            print(f"Error setting CPU affinity for PID {pid}: {e}")
            return False

    @staticmethod
    def get_cpu_affinity(pid: int) -> Optional[List[int]]:
        """Получение текущей CPU affinity"""
        try:
            if platform.system() != "Linux":
                return None
            result = subprocess.run(
                ['taskset', '-cp', str(pid)],
                capture_output=True, text=True
            )
            if result.returncode == 0 and 'affinity list:' in result.stdout:
                cpus_str = result.stdout.split('affinity list:')[1].strip()
                return [int(cpu) for cpu in cpus_str.split(',')]
            return None
        except Exception as e:
            print(f"Error getting CPU affinity for PID {pid}: {e}")
            return None

    @staticmethod
    def get_nice(pid: int) -> Optional[int]:
        """Получение текущего значения nice"""
        try:
            if platform.system() == "Windows":
                return None
            result = subprocess.run(
                ['ps', '-o', 'nice=', '-p', str(pid)],
                capture_output=True, text=True
            )
            if result.returncode == 0 and result.stdout.strip():
                return int(result.stdout.strip())
            return None
        except Exception as e:
            print(f"Error getting nice for PID {pid}: {e}")
            return None

    @staticmethod
    def get_cpu_count() -> int:
        """Получение количества CPU ядер"""
        return os.cpu_count()
