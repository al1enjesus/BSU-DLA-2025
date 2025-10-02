#!/usr/bin/env python3
"""pstat: краткая сводка по /proc/<pid>

Читает: /proc/<pid>/stat, /proc/<pid>/status, /proc/<pid>/io, /proc/<pid>/smaps_rollup (если есть)
Выводит: PPid, Threads, State, utime/stime (ticks), CPU time sec = (utime+stime)/HZ,
voluntary_ctxt_switches, nonvoluntary_ctxt_switches, VmRSS, RssAnon, RssFile,
read_bytes, write_bytes — с форматированием.

Пример использования:
  ./pstat.py <pid>
"""

import os
import sys
import argparse


def read(path):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return f.read()
    except Exception:
        return None


def parse_stat(pid):
    s = read(f"/proc/{pid}/stat")
    if not s:
        return None
    s = s.strip()
    # comm can contain spaces and is enclosed in parentheses — разбор по правому ')'
    rp = s.rfind(')')
    before = s[: rp + 1]
    after = s[rp + 2 :]
    parts = before.split(' ')
    parts += after.split()
    return parts


def parse_status(pid):
    s = read(f"/proc/{pid}/status")
    d = {}
    if not s:
        return d
    for line in s.splitlines():
        if ':' in line:
            k, v = line.split(':', 1)
            d[k.strip()] = v.strip()
    return d


def parse_io(pid):
    s = read(f"/proc/{pid}/io")
    d = {}
    if not s:
        return d
    for line in s.splitlines():
        if ':' in line:
            k, v = line.split(':', 1)
            d[k.strip()] = v.strip()
    return d


def parse_smaps_rollup(pid):
    s = read(f"/proc/{pid}/smaps_rollup")
    d = {}
    if not s:
        return d
    for line in s.splitlines():
        if ':' in line:
            k, v = line.split(':', 1)
            d[k.strip()] = v.strip()
    return d


def human_bytes(n):
    if n is None:
        return "N/A"
    try:
        n = int(n)
    except Exception:
        try:
            n = int(str(n).split()[0])
        except Exception:
            return str(n)
    # n in bytes
    if n < 1024:
        return f"{n} B"
    n = float(n)
    for unit in ["KiB", "MiB", "GiB", "TiB"]:
        n /= 1024.0
        if abs(n) < 1024.0:
            return f"{n:.3f} {unit}"
    return f"{n:.3f} PiB"


def format_kb(kb):
    if kb is None:
        return "N/A"
    try:
        v = int(kb)
    except Exception:
        try:
            v = int(str(kb).split()[0])
        except Exception:
            return str(kb)
    mib = v / 1024.0
    return f"{v} KiB ({mib:.3f} MiB)"


def main():
    parser = argparse.ArgumentParser(description="pstat: summary of /proc/<pid>")
    parser.add_argument('pid', help='PID to inspect')
    args = parser.parse_args()
    pid = args.pid
    if not pid.isdigit():
        print("pid must be numeric", file=sys.stderr)
        sys.exit(2)

    stat = parse_stat(pid)
    status = parse_status(pid)
    io = parse_io(pid)
    smaps = parse_smaps_rollup(pid)

    if not stat:
        print(f"Cannot read /proc/{pid}/stat. Process may not exist or permission denied.")
        sys.exit(1)

    # PID, PPid, Threads, State
    ppid = status.get('PPid', stat[3] if len(stat) > 3 else 'N/A')
    threads = status.get('Threads', stat[19] if len(stat) > 19 else 'N/A')
    state = stat[2] if len(stat) > 2 else status.get('State', 'N/A')

    # utime/stime are clock ticks (field 14/15 in /proc/<pid>/stat)
    try:
        utime = int(stat[13])
        stime = int(stat[14])
    except Exception:
        utime = 'N/A'
        stime = 'N/A'

    try:
        hz = os.sysconf(os.sysconf_names['SC_CLK_TCK'])
    except Exception:
        hz = 100

    cpu_time_sec = (utime + stime) / hz if isinstance(utime, int) and isinstance(stime, int) else 'N/A'

    voluntary = status.get('voluntary_ctxt_switches', 'N/A')
    nonvol = status.get('nonvoluntary_ctxt_switches', 'N/A')

    vmrss = status.get('VmRSS', 'N/A')
    vmrss_val = None
    if vmrss != 'N/A':
        vmrss_val = vmrss.split()[0]

    rss_anon = smaps.get('RssAnon', 'N/A')
    rss_file = smaps.get('RssFile', 'N/A')

    read_bytes = io.get('read_bytes', 'N/A')
    write_bytes = io.get('write_bytes', 'N/A')

    # Вывод
    print(f"PID: {pid}")
    print(f"PPid: {ppid}")
    print(f"Threads: {threads}")
    print(f"State: {state}")
    print(f"utime: {utime} ticks")
    print(f"stime: {stime} ticks")
    print(f"CPU time sec: {cpu_time_sec} sec (HZ={hz})")
    print(f"voluntary_ctxt_switches: {voluntary}")
    print(f"nonvoluntary_ctxt_switches: {nonvol}")
    if vmrss_val:
        print(f"VmRSS: {format_kb(vmrss_val)}")
    else:
        print(f"VmRSS: {vmrss}")
    print(f"RssAnon: {rss_anon}")
    print(f"RssFile: {rss_file}")
    # read_bytes/write_bytes are in bytes
    try:
        rb = int(read_bytes)
    except Exception:
        rb = None
    try:
        wb = int(write_bytes)
    except Exception:
        wb = None
    print(f"read_bytes: {read_bytes} bytes ({human_bytes(rb)})")
    print(f"write_bytes: {write_bytes} bytes ({human_bytes(wb)})")


if __name__ == '__main__':
    main()