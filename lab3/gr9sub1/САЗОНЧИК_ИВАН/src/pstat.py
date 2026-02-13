#!/usr/bin/env python3
"""
Утилита pstat для извлечения метрик процессов из /proc
"""

import os
import sys
import argparse
from pathlib import Path
from typing import Dict, Optional, Tuple

class ProcParser:
    """Парсер файлов /proc"""
    
    HZ = 100  # Частота тиков ядра
    
    @staticmethod
    def read_proc_file(pid: int, filename: str) -> Optional[str]:
        """Чтение файла из /proc"""
        try:
            file_path = Path(f"/proc/{pid}/{filename}")
            if file_path.exists():
                return file_path.read_text().strip()
        except (PermissionError, FileNotFoundError, IOError):
            pass
        return None
    
    @staticmethod
    def parse_proc_stat(pid: int) -> Dict:
        """Парсинг /proc/<pid>/stat"""
        data = {}
        content = ProcParser.read_proc_file(pid, "stat")
        if not content:
            return data
            
        # Формат: pid (comm) state ppid ... utime stime ... threads ...
        fields = content.split()
        if len(fields) < 24:
            return data
            
        data.update({
            'pid': int(fields[0]),
            'state': fields[2],
            'ppid': int(fields[3]),
            'utime': int(fields[13]),
            'stime': int(fields[14]),
            'threads': int(fields[19])
        })
        
        return data
    
    @staticmethod
    def parse_proc_status(pid: int) -> Dict:
        """Парсинг /proc/<pid>/status"""
        data = {}
        content = ProcParser.read_proc_file(pid, "status")
        if not content:
            return data
            
        for line in content.split('\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip()
                
                if key == 'VmRSS':
                    # Убираем 'kB' и преобразуем в число
                    data['vm_rss'] = int(value.replace('kB', '').strip())
                elif key == 'voluntary_ctxt_switches':
                    data['voluntary_ctxt_switches'] = int(value)
                elif key == 'nonvoluntary_ctxt_switches':
                    data['nonvoluntary_ctxt_switches'] = int(value)
        
        return data
    
    @staticmethod
    def parse_proc_io(pid: int) -> Dict:
        """Парсинг /proc/<pid>/io"""
        data = {}
        content = ProcParser.read_proc_file(pid, "io")
        if not content:
            return data
            
        for line in content.split('\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip()
                
                if key == 'read_bytes':
                    data['read_bytes'] = int(value)
                elif key == 'write_bytes':
                    data['write_bytes'] = int(value)
        
        return data
    
    @staticmethod
    def parse_smaps_rollup(pid: int) -> Dict:
        """Парсинг /proc/<pid>/smaps_rollup (если доступен)"""
        data = {'rss_anon': 0, 'rss_file': 0}
        content = ProcParser.read_proc_file(pid, "smaps_rollup")
        if not content:
            return data
            
        for line in content.split('\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip().replace('kB', '').strip()
                
                if key == 'Anonymous':
                    data['rss_anon'] = int(value)
                elif key == 'File':
                    data['rss_file'] = int(value)
        
        return data
    
    @staticmethod
    def get_process_info(pid: int) -> Dict:
        """Получение полной информации о процессе"""
        info = {}
        
        # Собираем данные из всех источников
        info.update(ProcParser.parse_proc_stat(pid))
        info.update(ProcParser.parse_proc_status(pid))
        info.update(ProcParser.parse_proc_io(pid))
        info.update(ProcParser.parse_smaps_rollup(pid))
        
        # Вычисляемые поля
        if 'utime' in info and 'stime' in info:
            info['cpu_time_sec'] = (info['utime'] + info['stime']) / ProcParser.HZ
        
        if 'vm_rss' in info:
            info['rss_mib'] = info['vm_rss'] / 1024.0
        
        return info


class Formatter:
    """Форматирование вывода"""
    
    @staticmethod
    def format_memory(kb: int) -> str:
        """Форматирование размера памяти"""
        if kb >= 1024:
            return f"{kb / 1024.0:.2f} MiB"
        return f"{kb} KiB"
    
    @staticmethod
    def format_bytes(bytes_count: int) -> str:
        """Форматирование байтов"""
        for unit in ['B', 'KiB', 'MiB', 'GiB']:
            if bytes_count < 1024.0:
                return f"{bytes_count:.2f} {unit}"
            bytes_count /= 1024.0
        return f"{bytes_count:.2f} TiB"
    
    @staticmethod
    def state_to_string(state: str) -> str:
        """Преобразование кода состояния в строку"""
        states = {
            'R': 'Running',
            'S': 'Sleeping',
            'D': 'Disk Sleep',
            'Z': 'Zombie',
            'T': 'Stopped',
            't': 'Tracing stop',
            'X': 'Dead',
            'x': 'Dead',
            'K': 'Wakekill',
            'W': 'Waking',
            'P': 'Parked'
        }
        return states.get(state, 'Unknown')


class PStat:
    """Основной класс утилиты"""
    
    def __init__(self):
        self.parser = ProcParser()
        self.formatter = Formatter()
    
    def print_info(self, pid: int):
        """Вывод информации о процессе"""
        info = self.parser.get_process_info(pid)
        
        if not info:
            print(f"Error: Cannot access process {pid} or process doesn't exist")
            return 1
        
        print(f"Process {pid} information:")
        print("=" * 50)
        
        # Основная информация
        print(f"{'PPid:':<30} {info.get('ppid', 'N/A')}")
        print(f"{'Threads:':<30} {info.get('threads', 'N/A')}")
        
        state = info.get('state', 'N/A')
        state_str = self.formatter.state_to_string(state)
        print(f"{'State:':<30} {state} ({state_str})")
        
        # Время CPU
        utime = info.get('utime', 'N/A')
        stime = info.get('stime', 'N/A')
        print(f"{'utime/stime:':<30} {utime}/{stime} ticks")
        
        cpu_time = info.get('cpu_time_sec', 'N/A')
        if cpu_time != 'N/A':
            print(f"{'CPU time sec:':<30} {cpu_time:.2f} s")
        
        # Контекстные переключения
        voluntary = info.get('voluntary_ctxt_switches', 'N/A')
        nonvoluntary = info.get('nonvoluntary_ctxt_switches', 'N/A')
        print(f"{'voluntary_ctxt_switches:':<30} {voluntary}")
        print(f"{'nonvoluntary_ctxt_switches:':<30} {nonvoluntary}")
        
        # Память
        vm_rss = info.get('vm_rss', 'N/A')
        if vm_rss != 'N/A':
            print(f"{'VmRSS:':<30} {vm_rss} KB")
            rss_mib = info.get('rss_mib', 'N/A')
            if rss_mib != 'N/A':
                print(f"{'RSS MiB:':<30} {rss_mib:.2f} MiB")
        
        # Детальная информация о памяти
        rss_anon = info.get('rss_anon', 'N/A')
        if rss_anon != 'N/A':
            print(f"{'RssAnon:':<30} {self.formatter.format_memory(rss_anon)}")
        
        rss_file = info.get('rss_file', 'N/A')
        if rss_file != 'N/A':
            print(f"{'RssFile:':<30} {self.formatter.format_memory(rss_file)}")
        
        # IO
        read_bytes = info.get('read_bytes', 'N/A')
        if read_bytes != 'N/A':
            print(f"{'read_bytes:':<30} {self.formatter.format_bytes(read_bytes)}")
        
        write_bytes = info.get('write_bytes', 'N/A')
        if write_bytes != 'N/A':
            print(f"{'write_bytes:':<30} {self.formatter.format_bytes(write_bytes)}")
        
        return 0
    
    def validate_pid(self, pid: str) -> Tuple[bool, int]:
        """Валидация PID"""
        try:
            pid_int = int(pid)
            if pid_int <= 0:
                return False, 0
            
            # Проверяем существование процесса
            proc_path = Path(f"/proc/{pid_int}")
            if not proc_path.exists():
                return False, 0
                
            return True, pid_int
        except ValueError:
            return False, 0


def main():
    """Основная функция"""
    parser = argparse.ArgumentParser(description='Display process information from /proc')
    parser.add_argument('pid', help='Process ID')
    
    args = parser.parse_args()
    
    pstat = PStat()
    valid, pid = pstat.validate_pid(args.pid)
    
    if not valid:
        print(f"Error: Invalid PID or process doesn't exist: {args.pid}")
        return 1
    
    return pstat.print_info(pid)


if __name__ == '__main__':
    sys.exit(main())