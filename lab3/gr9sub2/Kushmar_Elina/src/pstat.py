#!/usr/bin/env python3
import sys
import os

HZ = 100

def print_usage():
    print("Usage: pstat <pid>")

def get_value_from_file(filename, key):
    try:
        with open(filename, 'r') as f:
            for line in f:
                if line.startswith(key):
                    return int(line.split(':')[1].strip().split()[0])
    except:
        return None
    return None

def read_stat_file(pid):
    try:
        with open(f"/proc/{pid}/stat", 'r') as f:
            data = f.read().split()

        state = data[2]
        ppid = int(data[3])
        utime = int(data[13])
        stime = int(data[14])
        threads = int(data[19])

        print(f"PPid: {ppid}")
        print(f"State: {state}")
        print(f"Threads: {threads}")
        print(f"utime: {utime}")
        print(f"stime: {stime}")

        cpu_time_sec = (utime + stime) / HZ
        print(f"CPU time sec: {cpu_time_sec:.2f}")

    except Exception as e:
        print(f"Error reading stat file: {e}")

def read_status_file(pid):
    vm_rss = get_value_from_file(f"/proc/{pid}/status", "VmRSS")
    if vm_rss is not None:
        rss_mib = vm_rss / 1024.0
        print(f"VmRSS: {vm_rss} kB")
        print(f"RSS MiB: {rss_mib:.2f}")

    voluntary = get_value_from_file(f"/proc/{pid}/status", "voluntary_ctxt_switches")
    nonvoluntary = get_value_from_file(f"/proc/{pid}/status", "nonvoluntary_ctxt_switches")

    if voluntary is not None:
        print(f"voluntary_ctxt_switches: {voluntary}")
    if nonvoluntary is not None:
        print(f"nonvoluntary_ctxt_switches: {nonvoluntary}")

def read_io_file(pid):
    read_bytes = get_value_from_file(f"/proc/{pid}/io", "read_bytes")
    write_bytes = get_value_from_file(f"/proc/{pid}/io", "write_bytes")

    if read_bytes is not None:
        print(f"read_bytes: {read_bytes}")
    if write_bytes is not None:
        print(f"write_bytes: {write_bytes}")

def read_smaps_rollup(pid):
    rss_anon = get_value_from_file(f"/proc/{pid}/smaps_rollup", "RssAnon")
    rss_file = get_value_from_file(f"/proc/{pid}/smaps_rollup", "RssFile")

    if rss_anon is not None:
        print(f"RssAnon: {rss_anon} kB")
    if rss_file is not None:
        print(f"RssFile: {rss_file} kB")

def main():
    if len(sys.argv) != 2:
        print_usage()
        sys.exit(1)

    pid = sys.argv[1]
    if not pid.isdigit():
        print("Invalid PID")
        sys.exit(1)

    pid = int(pid)

    if not os.path.exists(f"/proc/{pid}"):
        print(f"Process {pid} does not exist")
        sys.exit(1)

    print(f"PID: {pid}")
    read_stat_file(pid)
    read_status_file(pid)
    read_io_file(pid)
    read_smaps_rollup(pid)

if __name__ == "__main__":
    main()