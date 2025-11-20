import sys
import os

def print_human_readable_kb(kb):
    if kb > 1024:
        print(f"{kb / 1024:.2f} MiB", end="")
    else:
        print(f"{kb} KiB", end="")

def read_stat(pid):
    path = f"/proc/{pid}/stat"
    try:
        with open(path) as f:
            parts = f.read().split()
            comm = parts[1].strip("()")
            state = parts[2]
            ppid = int(parts[3])
            utime = int(parts[13])
            stime = int(parts[14])
            hz = os.sysconf(os.sysconf_names['SC_CLK_TCK'])
            cpu_time = (utime + stime) / hz
            return comm, state, ppid, cpu_time
    except Exception as e:
        print(f"[ERROR] stat: {e}")
        sys.exit(1)

def read_status(pid):
    path = f"/proc/{pid}/status"
    data = {
        "Threads": 0,
        "VmRSS": 0,
        "RssAnon": 0,
        "RssFile": 0,
        "voluntary_ctxt_switches": 0,
        "nonvoluntary_ctxt_switches": 0
    }
    try:
        with open(path) as f:
            for line in f:
                for key in data:
                    if line.startswith(key + ":"):
                        value = int(line.split()[1])
                        data[key] = value
        return data
    except Exception as e:
        print(f"[ERROR] status: {e}")
        sys.exit(1)

def read_io(pid):
    path = f"/proc/{pid}/io"
    read_bytes = write_bytes = 0
    try:
        with open(path) as f:
            for line in f:
                if line.startswith("read_bytes:"):
                    read_bytes = int(line.split()[1])
                elif line.startswith("write_bytes:"):
                    write_bytes = int(line.split()[1])
        return read_bytes, write_bytes
    except Exception as e:
        print(f"[ERROR] io: {e}")
        sys.exit(1)

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <pid>")
        sys.exit(1)

    pid = int(sys.argv[1])
    comm, state, ppid, cpu_time = read_stat(pid)
    status = read_status(pid)
    read_bytes, write_bytes = read_io(pid)

    print(f"Process {pid} ({comm})")
    print(f"  PPid: {ppid}")
    print(f"  State: {state}")
    print(f"  Threads: {status['Threads']}")
    print(f"  CPU time: {cpu_time:.2f} sec (utime+stime)")
    print(f"  Context switches: voluntary={status['voluntary_ctxt_switches']}, nonvoluntary={status['nonvoluntary_ctxt_switches']}")

    print("  Memory:")
    print("    VmRSS: ", end=""); print_human_readable_kb(status['VmRSS']); print()
    print("    RssAnon: ", end=""); print_human_readable_kb(status['RssAnon']); print()
    print("    RssFile: ", end=""); print_human_readable_kb(status['RssFile']); print()

    print("  IO:")
    print(f"    read_bytes: {read_bytes}")
    print(f"    write_bytes: {write_bytes}")

if __name__ == "__main__":
    main()

