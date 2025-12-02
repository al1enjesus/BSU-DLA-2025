import sys
import os
try:
    HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])
except (KeyError, AttributeError):
    HZ = 100

STAT_INDICES = {
    'comm': 1,
    'state': 2,
    'ppid': 3,
    'utime': 13,
    'stime': 14,
    'starttime': 21
}

def parse_kb_value(value_str):
    """Парсит строку вида '123 kB' в целое число килобайт"""
    if not value_str:
        return 0
    try:
        if " kB" in value_str:
            return int(value_str.replace(" kB", "").strip())
        else:
            return int(value_str.split()[0])
    except (ValueError, IndexError):
        return 0

def format_size(kb: int) -> str:
    """Форматирование KiB → MiB/KiB"""
    if kb >= 1024:
        return f"{kb/1024:.2f} MiB"
    return f"{kb} KiB"

def read_stat(pid):
    """Чтение /proc/pid/stat с обработкой ошибок"""
    try:
        with open(f"/proc/{pid}/stat") as f:
            fields = f.read().split()
        return fields
    except (FileNotFoundError, PermissionError) as e:
        print(f"Error reading /proc/{pid}/stat: {e}")
        sys.exit(1)

def read_status(pid):
    """Чтение /proc/pid/status"""
    data = {}
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if ":" in line:
                    k, v = line.split(":", 1)
                    data[k.strip()] = v.strip()
    except (FileNotFoundError, PermissionError):
        pass
    return data

def read_io(pid):
    """Чтение /proc/pid/io"""
    data = {}
    path = f"/proc/{pid}/io"
    if os.path.exists(path):
        try:
            with open(path) as f:
                for line in f:
                    if ":" in line:
                        k, v = line.split(":", 1)
                        data[k.strip()] = int(v.strip())
        except (PermissionError, ValueError):
            pass
    return data

def read_smaps_rollup(pid):
    """Чтение /proc/pid/smaps_rollup (может отсутствовать)"""
    data = {}
    path = f"/proc/{pid}/smaps_rollup"
    if os.path.exists(path):
        try:
            with open(path) as f:
                for line in f:
                    if ":" in line:
                        k, v = line.split(":", 1)
                        data[k.strip()] = parse_kb_value(v.strip())
        except (PermissionError, ValueError):
            pass
    return data

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <pid>")
        sys.exit(1)

    pid = sys.argv[1]
    if not os.path.exists(f"/proc/{pid}"):
        print(f"Error: Process {pid} does not exist")
        sys.exit(1)

    stat = read_stat(pid)
    comm = stat[STAT_INDICES['comm']].strip("()")
    state = stat[STAT_INDICES['state']]
    ppid = stat[STAT_INDICES['ppid']]
    utime = int(stat[STAT_INDICES['utime']])
    stime = int(stat[STAT_INDICES['stime']])
    cpu_time = (utime + stime) / HZ

    status = read_status(pid)
    threads = status.get("Threads", "?")
    voluntary_ctxt = status.get("voluntary_ctxt_switches", "?")
    nonvoluntary_ctxt = status.get("nonvoluntary_ctxt_switches", "?")
    vmrss = status.get("VmRSS", "0 kB")

    smaps = read_smaps_rollup(pid)
    rss_anon = smaps.get("Anonymous", 0)
    rss_file = smaps.get("Pss_File", 0)

    io = read_io(pid)
    read_bytes = io.get("read_bytes", 0)
    write_bytes = io.get("write_bytes", 0)

    # Вывод результатов
    print(f"PID: {pid} ({comm})")
    print(f"PPid: {ppid}")
    print(f"State: {state}")
    print(f"Threads: {threads}")
    print(f"CPU time: {cpu_time:.2f} sec (HZ={HZ})")
    print(f"Voluntary ctxt switches: {voluntary_ctxt}")
    print(f"Nonvoluntary ctxt switches: {nonvoluntary_ctxt}")

    try:
        vmrss_kb = int(vmrss.split()[0])
    except (ValueError, IndexError, AttributeError):
        vmrss_kb = 0
    print(f"VmRSS: {format_size(vmrss_kb)}")
    print(f"  RssAnon: {format_size(rss_anon)}")
    print(f"  RssFile: {format_size(rss_file)}")

    print("IO:")
    print(f"  read_bytes: {format_size(read_bytes // 1024)}")
    print(f"  write_bytes: {format_size(write_bytes // 1024)}")

if __name__ == "__main__":
    main()