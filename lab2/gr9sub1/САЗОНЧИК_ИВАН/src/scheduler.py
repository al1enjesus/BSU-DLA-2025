#!/usr/bin/env python3
import os
import sys
import platform
from typing import List, Optional

class ProcessScheduler:
    """Класс для управления планированием процессов"""
    
    @staticmethod
    def set_nice(pid: int, nice_value: int) -> bool:
        """Установка значения nice для процесса"""
        try:
            if platform.system() == "Windows":
                print(f"Warning: nice not supported on Windows")
                return False
            
            # Используем os.nice для текущего процесса или внешние утилиты для других
            if pid == os.getpid():
                # Для текущего процесса
                os.nice(nice_value - os.nice(0))
                return True
            else:
                # Для других процессов используем renice
                import subprocess
                result = subprocess.run(
                    ['renice', str(nice_value), '-p', str(pid)],
                    capture_output=True, text=True
                )
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
            
            # Используем taskset для установки affinity
            import subprocess
            cpu_mask = ','.join(map(str, cpus))
            result = subprocess.run(
                ['taskset', '-cp', cpu_mask, str(pid)],
                capture_output=True, text=True
            )
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
            
            import subprocess
            result = subprocess.run(
                ['taskset', '-cp', str(pid)],
                capture_output=True, text=True
            )
            
            if result.returncode == 0:
                # Парсим вывод: pid 12345's current affinity list: 0,1
                output = result.stdout.strip()
                if 'affinity list:' in output:
                    cpus_str = output.split('affinity list:')[1].strip()
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
            
            import subprocess
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