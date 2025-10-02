#!/usr/bin/env python3
import sys
import os

HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

def format_bytes(kb):
    if kb > 1024 * 1024:
        return f"{kb/1024/1024:.2f} GiB"
    elif kb > 1024:
        return f"{kb/1024:.2f} MiB"
    else:
        return f"{kb:.2f} KiB"

def parse_stat(path):
    with open(path) as f:
        data = f.read().split()
    return {
        "pid": int(data[0]),
        "comm": data[1].strip("()"),
        "state": data[2],
        "ppid": int(data[3]),
        "utime": int(data[13]),
        "stime": int(data[14])
    }

def parse_status(path):
    res = {}
    with open(path) as f:
        for line in f:
            if ":" in line:
                k,v = line.split(":",1)
                res[k.strip()] = v.strip()
    return res

def parse_io(path):
    res = {}
    try:
        with open(path) as f:
            for line in f:
                k,v = line.split(":",1)
                res[k.strip()] = int(v.strip())
    except FileNotFoundError:
        pass
    return res

def parse_smaps(path):
    res = {}
    try:
        with open(path) as f:
            for line in f:
                if line.startswith("RssAnon"):
                    res["RssAnon"] = int(line.split()[1])
                elif line.startswith("RssFile"):
                    res["RssFile"] = int(line.split()[1])
                elif line.startswith("RssShmem"):
                    res["RssShmem"] = int(line.split()[1])
    except FileNotFoundError:
        pass
    return res

def main():
    if len(sys.argv) != 2:
        print("Usage: pstat <pid>")
        sys.exit(1)

    pid = sys.argv[1]
    stat = parse_stat(f"/proc/{pid}/stat")
    status = parse_status(f"/proc/{pid}/status")
    io = parse_io(f"/proc/{pid}/io")
    smaps = parse_smaps(f"/proc/{pid}/smaps_rollup")

    utime = stat["utime"] / HZ
    stime = stat["stime"] / HZ
    cpu_time = utime + stime

    print(f"Process {stat['comm']} (pid={pid})")
    print(f"  PPid: {stat['ppid']}")
    print(f"  State: {stat['state']}")
    print(f"  Threads: {status.get('Threads')}")
    print(f"  CPU time: user={utime:.2f}s sys={stime:.2f}s total={cpu_time:.2f}s")
    print(f"  Voluntary ctxt switches: {status.get('voluntary_ctxt_switches')}")
    print(f"  Nonvoluntary ctxt switches: {status.get('nonvoluntary_ctxt_switches')}")
    print(f"  VmRSS: {status.get('VmRSS')}")
    if smaps:
        rss_anon = smaps.get("RssAnon", 0)
        rss_file = smaps.get("RssFile", 0)
        print(f"  RssAnon: {rss_anon} kB ({format_bytes(rss_anon)})")
        print(f"  RssFile: {rss_file} kB ({format_bytes(rss_file)})")
    if io:
        print(f"  read_bytes: {io.get('read_bytes',0)}")
        print(f"  write_bytes: {io.get('write_bytes',0)}")

if __name__ == "__main__":
    main()
