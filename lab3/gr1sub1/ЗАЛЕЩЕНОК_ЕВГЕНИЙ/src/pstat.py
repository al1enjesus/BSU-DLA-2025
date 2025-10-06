import sys
import os

def log_error(msg):
    sys.stderr.write(f"Error: {msg}\n")

def get_hz():
    """Получает значение HZ (системных тиков в секунду)"""
    try:
        return os.sysconf(os.sysconf_names['SC_CLK_TCK'])
    except (ValueError, AttributeError) as e:
        sys.stderr.write(f"Warning: Не удалось получить SC_CLK_TCK, используется 100. ({e})\n")
        return 100 

def format_bytes(byte_count):
    """Форматирует байты в КиБ или МиБ"""
    if byte_count is None:
        return "N/A"
    try:
        val = float(byte_count)
    except (TypeError, ValueError):
        return "N/A"
        
    if val > 1024 * 1024:
        return f"{val / (1024 * 1024):.2f} MiB"
    elif val > 1024:
        return f"{val / 1024:.2f} KiB"
    return f"{val:.0f} B"

def parse_proc_files(pid):
    """Анализирует файлы в /proc/pid и возвращает словарь с метриками"""
    metrics = {}
    proc_path = f"/proc/{pid}"

    if not os.path.exists(proc_path) or not os.path.isdir(proc_path):
        log_error(f"Процесс с PID {pid} не найден (директория {proc_path} отсутствует).")
        return None

    try:
        with open(f"{proc_path}/status", "r") as f:
            for line in f:
                parts = line.split()
                key = parts[0].rstrip(':')
                
                if key == "PPid":
                    metrics['PPid'] = int(parts[1])
                elif key == "State":
                    metrics['State'] = parts[1] 
                elif key == "Threads":
                    metrics['Threads'] = int(parts[1])
                elif key == "VmRSS":
                    metrics['VmRSS'] = int(parts[1]) * 1024
                elif key == "RssAnon":
                    metrics['RssAnon'] = int(parts[1]) * 1024
                elif key == "RssFile":
                    metrics['RssFile'] = int(parts[1]) * 1024
                elif key == "voluntary_ctxt_switches":
                    metrics['voluntary_ctxt_switches'] = int(parts[1])
                elif key == "nonvoluntary_ctxt_switches":
                    metrics['nonvoluntary_ctxt_switches'] = int(parts[1])
    except FileNotFoundError:
        log_error(f"Процесс {pid} завершился во время чтения 'status'.")
        return None
    except PermissionError:
        log_error(f"Нет прав на чтение {proc_path}/status.")
        return None
    except Exception as e:
        log_error(f"Ошибка при парсинге status: {e}")

    try:
        with open(f"{proc_path}/stat", "r") as f:
            content = f.read()
            r_paren_idx = content.rfind(')')
            if r_paren_idx != -1:
                rest_of_line = content[r_paren_idx+2:].split()
                metrics['utime'] = int(rest_of_line[11]) 
                metrics['stime'] = int(rest_of_line[12])
            else:
                 log_error("Не удалось распарсить формат /proc/<pid>/stat")
    except FileNotFoundError:
        log_error(f"Процесс {pid} завершился во время чтения 'stat'.")
        return None
    except Exception as e:
        log_error(f"Ошибка при чтении/парсинге stat: {e}")
        
    metrics['read_bytes'] = None
    metrics['write_bytes'] = None
    try:
        with open(f"{proc_path}/io", "r") as f:
            for line in f:
                if line.startswith("read_bytes:"):
                    metrics['read_bytes'] = int(line.split()[1])
                elif line.startswith("write_bytes:"):
                    metrics['write_bytes'] = int(line.split()[1])
    except PermissionError:
        sys.stderr.write(f"Info: Нет прав на чтение {proc_path}/io (нужен root/владелец). I/O данные недоступны.\n")
    except FileNotFoundError:
         pass 

    return metrics

def main():
    if len(sys.argv) != 2:
        print(f"Использование: {sys.argv[0]} <pid>")
        sys.exit(1)
        
    pid_str = sys.argv[1]
    if not pid_str.isdigit():
        log_error("PID должен быть положительным целым числом.")
        sys.exit(1)
    
    pid = int(pid_str)
    data = parse_proc_files(pid)
    
    if data is None:
        sys.exit(1)

    required_fields = ['State', 'utime', 'stime', 'VmRSS']
    missing = [field for field in required_fields if field not in data]
    if missing:
        log_error(f"Не удалось получить критические метрики: {missing}. Возможно, процесс завершается.")
        sys.exit(1)

    hz = get_hz()
    
    rss_mib = data['VmRSS'] / (1024 * 1024)
    cpu_time_sec = (data['utime'] + data['stime']) / hz


    print(f"--- Статистика для процесса PID: {pid} ---")
    print(f"  PPid:                     {data.get('PPid', 'N/A')}")
    print(f"  Количество потоков:       {data.get('Threads', 'N/A')}")
    print(f"  Состояние:                {data['State']}") 
    print("-" * 20)
    print(f"  Время CPU (user):         {data['utime'] / hz:.2f} сек.")
    print(f"  Время CPU (kernel):       {data['stime'] / hz:.2f} сек.")
    print(f"  Общее время CPU:          {cpu_time_sec:.2f} сек. (HZ={hz})")
    print("-" * 20)
    print(f"  Добровольные перекл.:     {data.get('voluntary_ctxt_switches', 'N/A')}")
    print(f"  Недобровольные перекл.:   {data.get('nonvoluntary_ctxt_switches', 'N/A')}")
    print("-" * 20)
    print(f"  VmRSS (общий):            {format_bytes(data['VmRSS'])} ({rss_mib:.2f} MiB)")
    print(f"  RssAnon (анонимная):      {format_bytes(data.get('RssAnon'))}")
    print(f"  RssFile (файловая):       {format_bytes(data.get('RssFile'))}")
    print("-" * 20)
    print(f"  Байт прочитано (I/O):     {format_bytes(data.get('read_bytes'))}")
    print(f"  Байт записано (I/O):      {format_bytes(data.get('write_bytes'))}")
    print("-" * 40)

if __name__ == "__main__":
    main()
