#!/usr/bin/env python3
import sys
import os

HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

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
    with open(f"/proc/{pid}/stat") as f:
        fields = f.read().split()
    return fields

def read_status(pid):
    data = {}
    with open(f"/proc/{pid}/status") as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                data[k.strip()] = v.strip()
    return data

def read_io(pid):
    data = {}
    path = f"/proc/{pid}/io"
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                k, v = line.split(":", 1)
                data[k.strip()] = int(v.strip())
    return data

def read_smaps_rollup(pid):
    data = {}
    path = f"/proc/{pid}/smaps_rollup"
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                if ":" in line:
                    k, v = line.split(":", 1)
                    data[k.strip()] = parse_kb_value(v.strip())
    return data

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <pid>")
        sys.exit(1)
    pid = sys.argv[1]

    stat = read_stat(pid)
    comm = stat[1].strip("()")
    state = stat[2]
    ppid = stat[3]
    utime = int(stat[13])
    stime = int(stat[14])
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

    print(f"PID: {pid} ({comm})")
    print(f"PPid: {ppid}")
    print(f"State: {state}")
    print(f"Threads: {threads}")
    print(f"CPU time: {cpu_time:.2f} sec")
    print(f"Voluntary ctxt switches: {voluntary_ctxt}")
    print(f"Nonvoluntary ctxt switches: {nonvoluntary_ctxt}")

    try:
        vmrss_kb = int(vmrss.split()[0])
    except:
        vmrss_kb = 0
    print(f"VmRSS: {format_size(vmrss_kb)}")
    print(f"  RssAnon: {format_size(rss_anon)}")
    print(f"  RssFile: {format_size(rss_file)}")

    print("IO:")
    print(f"  read_bytes: {format_size(read_bytes // 1024)}")
    print(f"  write_bytes: {format_size(write_bytes // 1024)}")

if __name__ == "__main__":
    main()

