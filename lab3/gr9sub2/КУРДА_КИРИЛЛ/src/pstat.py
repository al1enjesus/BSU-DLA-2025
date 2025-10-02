import sys
import os
from pathlib import Path

def format_bytes(bytes_val, unit='B'):
    """Форматирование байтов в удобный вид"""
    if bytes_val is None:
        return "N/A"
    
    for unit_name in ['', 'Ki', 'Mi', 'Gi', 'Ti']:
        if abs(bytes_val) < 1024.0:
            return f"{bytes_val:.2f} {unit_name}{unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.2f} Pi{unit}"

def parse_proc_stat(pid):
    """Парсинг /proc/<pid>/stat"""
    try:
        with open(f'/proc/{pid}/stat', 'r') as f:
            stat = f.read().strip()
        
        end_paren = stat.rfind(')')
        fields = stat[end_paren + 2:].split()
        
        state = fields[0]
        ppid = int(fields[1])
        utime = int(fields[11])
        stime = int(fields[12])
        num_threads = int(fields[17])
        
        return {
            'state': state,
            'ppid': ppid,
            'utime': utime,
            'stime': stime,
            'num_threads': num_threads
        }
    except (FileNotFoundError, IndexError, ValueError) as e:
        return None

def parse_proc_status(pid):
    """Парсинг /proc/<pid>/status"""
    try:
        status = {}
        with open(f'/proc/{pid}/status', 'r') as f:
            for line in f:
                if ':' in line:
                    key, value = line.split(':', 1)
                    status[key.strip()] = value.strip()
        
        result = {}
        
        if 'VmRSS' in status:
            result['VmRSS'] = int(status['VmRSS'].split()[0]) * 1024
        
        if 'RssAnon' in status:
            result['RssAnon'] = int(status['RssAnon'].split()[0]) * 1024
        
        if 'RssFile' in status:
            result['RssFile'] = int(status['RssFile'].split()[0]) * 1024
        
        # Переключения контекста
        if 'voluntary_ctxt_switches' in status:
            result['voluntary_ctxt_switches'] = int(status['voluntary_ctxt_switches'])
        
        if 'nonvoluntary_ctxt_switches' in status:
            result['nonvoluntary_ctxt_switches'] = int(status['nonvoluntary_ctxt_switches'])
        
        return result
    except (FileNotFoundError, ValueError) as e:
        return {}

def parse_proc_io(pid):
    """Парсинг /proc/<pid>/io"""
    try:
        io_stats = {}
        with open(f'/proc/{pid}/io', 'r') as f:
            for line in f:
                if ':' in line:
                    key, value = line.split(':', 1)
                    io_stats[key.strip()] = int(value.strip())
        
        return {
            'read_bytes': io_stats.get('read_bytes', 0),
            'write_bytes': io_stats.get('write_bytes', 0),
            'rchar': io_stats.get('rchar', 0),
            'wchar': io_stats.get('wchar', 0)
        }
    except (FileNotFoundError, ValueError, PermissionError) as e:
        return {}

def get_hz():
    """Получение значения HZ (тиков в секунду)"""
    try:
        return os.sysconf(os.sysconf_names['SC_CLK_TCK'])
    except:
        return 100

def main():
    if len(sys.argv) != 2:
        print(f"Использование: {sys.argv[0]} <pid>")
        print(f"Примеры:")
        print(f"  {sys.argv[0]} 1      # Анализ процесса init")
        print(f"  {sys.argv[0]} $$     # Анализ текущего shell")
        print(f"  {sys.argv[0]} self   # Анализ самого себя")
        sys.exit(1)
    
    pid_arg = sys.argv[1]
    
    if pid_arg == '$$':
        pid = os.getppid()
    elif pid_arg == 'self':
        pid = os.getpid()
    else:
        try:
            pid = int(pid_arg)
        except ValueError:
            pid = pid_arg
    
    if not os.path.exists(f'/proc/{pid}'):
        print(f"Ошибка: Процесс с PID {pid} не найден")
        sys.exit(1)
    
    print("\n" + "="*60)
    print(f"Process Statistics for PID: {pid}")
    print("="*60)
    
    try:
        with open(f'/proc/{pid}/comm', 'r') as f:
            comm = f.read().strip()
        print(f"Command: {comm}")
    except:
        pass
    
    try:
        with open(f'/proc/{pid}/cmdline', 'r') as f:
            cmdline = f.read().replace('\0', ' ').strip()
        if cmdline:
            print(f"Cmdline: {cmdline[:80]}{'...' if len(cmdline) > 80 else ''}")
    except:
        pass
    
    stat = parse_proc_stat(pid)
    if stat:
        print("\n--- Базовая информация ---")
        print(f"Parent PID: {stat['ppid']}")
        print(f"State: {stat['state']} ", end='')
        states = {
            'R': '(Running)',
            'S': '(Sleeping)',
            'D': '(Disk sleep)',
            'Z': '(Zombie)',
            'T': '(Stopped)',
            't': '(Tracing stop)',
            'X': '(Dead)',
            'I': '(Idle)'
        }
        print(states.get(stat['state'], ''))
        print(f"Threads: {stat['num_threads']}")
        
        hz = get_hz()
        cpu_time = (stat['utime'] + stat['stime']) / hz
        utime_sec = stat['utime'] / hz
        stime_sec = stat['stime'] / hz
        
        print("\n--- CPU время ---")
        print(f"User time: {utime_sec:.2f} sec")
        print(f"System time: {stime_sec:.2f} sec")
        print(f"Total CPU time: {cpu_time:.2f} sec")
        print(f"(HZ = {hz} ticks/sec)")
    
    status = parse_proc_status(pid)
    if status:
        print("\n--- Память ---")
        if 'VmRSS' in status:
            rss_mb = status['VmRSS'] / (1024 * 1024)
            print(f"VmRSS: {format_bytes(status['VmRSS'])} ({rss_mb:.2f} MiB)")
        
        if 'RssAnon' in status:
            print(f"RssAnon: {format_bytes(status['RssAnon'])}")
        
        if 'RssFile' in status:
            print(f"RssFile: {format_bytes(status['RssFile'])}")
        
        print("\n--- Переключения контекста ---")
        if 'voluntary_ctxt_switches' in status:
            print(f"Voluntary: {status['voluntary_ctxt_switches']}")
        
        if 'nonvoluntary_ctxt_switches' in status:
            print(f"Non-voluntary: {status['nonvoluntary_ctxt_switches']}")
    
    io = parse_proc_io(pid)
    if io:
        print("\n--- I/O статистика ---")
        print(f"Read bytes: {format_bytes(io.get('read_bytes', 0))}")
        print(f"Write bytes: {format_bytes(io.get('write_bytes', 0))}")
        print(f"Read chars: {format_bytes(io.get('rchar', 0))}")
        print(f"Write chars: {format_bytes(io.get('wchar', 0))}")
    elif str(pid) != 'self' and os.geteuid() != 0:
        print("\n--- I/O статистика ---")
        print("Недоступна (требуются права root или владельца процесса)")
    
    print("\n" + "="*60 + "\n")

if __name__ == '__main__':
    main()