#!/usr/bin/env python3

import sys
import os
import re

def get_hz():
    """Получает значение HZ (системных тиков в секунду)"""
    try:
        return os.sysconf(os.sysconf_names['SC_CLK_TCK'])
    except:
        return 100 # Значение по умолчанию

def format_bytes(byte_count):
    """Форматирует байты в КиБ или МиБ"""
    if byte_count is None:
        return "N/A"
    if byte_count > 1024 * 1024:
        return f"{byte_count / (1024 * 1024):.2f} MiB"
    elif byte_count > 1024:
        return f"{byte_count / 1024:.2f} KiB"
    return f"{byte_count} B"

def parse_proc_files(pid):
    """Анализирует файлы в /proc/pid и возвращает словарь с метриками"""
    metrics = {}
    
    # --- /proc/<pid>/status ---
    try:
        with open(f"/proc/{pid}/status", "r") as f:
            for line in f:
                if line.startswith("PPid:"):
                    metrics['PPid'] = int(line.split()[1])
                elif line.startswith("State:"):
                    metrics['State'] = line.split()[1]
                elif line.startswith("Threads:"):
                    metrics['Threads'] = int(line.split()[1])
                elif line.startswith("VmRSS:"):
                    metrics['VmRSS'] = int(line.split()[1]) * 1024
                elif line.startswith("RssAnon:"):
                    metrics['RssAnon'] = int(line.split()[1]) * 1024
                elif line.startswith("RssFile:"):
                    metrics['RssFile'] = int(line.split()[1]) * 1024
                elif line.startswith("voluntary_ctxt_switches:"):
                    metrics['voluntary_ctxt_switches'] = int(line.split()[1])
                elif line.startswith("nonvoluntary_ctxt_switches:"):
                    metrics['nonvoluntary_ctxt_switches'] = int(line.split()[1])
    except FileNotFoundError:
        print(f"Ошибка: Процесс с PID {pid} не найден.")
        sys.exit(1)
    except Exception as e:
        print(f"Ошибка при чтении /proc/{pid}/status: {e}")

    # --- /proc/<pid>/stat ---
    try:
        with open(f"/proc/{pid}/stat", "r") as f:
            parts = f.read().split()
            metrics['utime'] = int(parts[13])
            metrics['stime'] = int(parts[14])
    except Exception as e:
        print(f"Ошибка при чтении /proc/{pid}/stat: {e}")
        
    # --- /proc/<pid>/io ---
    try:
        with open(f"/proc/{pid}/io", "r") as f:
            for line in f:
                if line.startswith("read_bytes:"):
                    metrics['read_bytes'] = int(line.split()[1])
                elif line.startswith("write_bytes:"):
                    metrics['write_bytes'] = int(line.split()[1])
    except Exception as e:
        # Может быть недоступен без прав суперпользователя
        metrics['read_bytes'] = None
        metrics['write_bytes'] = None

    return metrics

def main():
    if len(sys.argv) != 2 or not sys.argv[1].isdigit():
        print("Использование: pstat <pid>")
        sys.exit(1)
        
    pid = sys.argv[1]
    data = parse_proc_files(pid)
    hz = get_hz()

    if not data:
        return
        
    rss_mib = data.get('VmRSS', 0) / (1024 * 1024)
    cpu_time_sec = (data.get('utime', 0) + data.get('stime', 0)) / hz

    print(f"--- Статистика для процесса PID: {pid} ---")
    print(f"  PPid:                     {data.get('PPid', 'N/A')}")
    print(f"  Количество потоков:       {data.get('Threads', 'N/A')}")
    print(f"  Состояние:                {data.get('State', 'N/A')}")
    print("-" * 20)
    print(f"  Время CPU (user):         {data.get('utime', 0) / hz:.2f} сек.")
    print(f"  Время CPU (kernel):       {data.get('stime', 0) / hz:.2f} сек.")
    print(f"  Общее время CPU:          {cpu_time_sec:.2f} сек.")
    print("-" * 20)
    print(f"  Добровольные перекл. контекста: {data.get('voluntary_ctxt_switches', 'N/A')}")
    print(f"  Недобровольные перекл. контекста: {data.get('nonvoluntary_ctxt_switches', 'N/A')}")
    print("-" * 20)
    print(f"  VmRSS (общий):            {format_bytes(data.get('VmRSS'))} ({rss_mib:.2f} MiB)")
    print(f"  RssAnon (анонимная):      {format_bytes(data.get('RssAnon'))}")
    print(f"  RssFile (файловая):       {format_bytes(data.get('RssFile'))}")
    print("-" * 20)
    print(f"  Байт прочитано (I/O):     {format_bytes(data.get('read_bytes'))}")
    print(f"  Байт записано (I/O):      {format_bytes(data.get('write_bytes'))}")
    print("-" * 40)

if __name__ == "__main__":
    main()
