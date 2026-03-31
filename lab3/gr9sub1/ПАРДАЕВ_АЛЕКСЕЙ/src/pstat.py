#!/usr/bin/env python3
import os
import sys

HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

def format_kib(val):
    return f"{val/1024:.2f} MiB" if val else "0 MiB"

def parse_stat(path):
    with open(path) as f:
        data = f.read().split()
    return {
        "pid": data[0],
        "comm": data[1].strip("()"),
        "state": data[2],
        "ppid": data[3],
        "utime": int(data[13]),
        "stime": int(data[14])
    }

def parse_status(path):
    info = {}
    with open(path) as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                info[k.strip()] = v.strip()
    return info

def parse_io(path):
    info = {}
    with open(path) as f:
        for line in f:
            if ":" in line:
                k, v = line.split(":", 1)
                info[k.strip()] = v.strip()
    return info

def pstat(pid):
    base = f"/proc/{pid}"
    if not os.path.exists(base):
        print(f"Process {pid} not found")
        return

    stat = parse_stat(f"{base}/stat")
    status = parse_status(f"{base}/status")
    io = parse_io(f"{base}/io")

    utime = stat["utime"] / HZ
    stime = stat["stime"] / HZ
    cpu_time = utime + stime

    print(f"PID: {stat['pid']} ({stat['comm']})")
    print(f"State: {stat['state']}, PPid: {stat['ppid']}")
    print(f"Threads: {status.get('Threads')}")
    print(f"CPU time: user={utime:.2f}s system={stime:.2f}s total={cpu_time:.2f}s")
    print(f"Voluntary CS: {status.get('voluntary_ctxt_switches')}")
    print(f"Nonvoluntary CS: {status.get('nonvoluntary_ctxt_switches')}")
    print(f"VmRSS: {status.get('VmRSS')} ({format_kib(int(status.get('VmRSS','0 kB').split()[0]))})")
    print(f"RssAnon: {status.get('RssAnon','N/A')}")
    print(f"RssFile: {status.get('RssFile','N/A')}")
    print(f"IO read_bytes: {io.get('read_bytes')} B")
    print(f"IO write_bytes: {io.get('write_bytes')} B")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: pstat.py <pid>")
        sys.exit(1)
    pstat(sys.argv[1])
