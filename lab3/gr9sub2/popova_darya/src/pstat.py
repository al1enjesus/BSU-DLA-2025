#!/usr/bin/env python3
"""
pstat_personal.py
A small personalized variant of pstat: prints a slightly different default summary
and runs system-tool comparisons by default. Use --no-compare to disable comparison.
"""
import os
import sys
import re
import subprocess
import argparse
import shutil

__version__ = "pstat-personal-1.0"

def read_file(path):
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            return f.read()
    except Exception:
        return None

# ---- parsers ----
def parse_stat(pid):
    s = read_file(f'/proc/{pid}/stat')
    if not s:
        return None
    i1 = s.find('(')
    i2 = s.rfind(')')
    pre = s[:i1].strip().split()
    comm = s[i1+1:i2]
    post = s[i2+1:].strip().split()
    fields = pre + [comm] + post
    utime = int(fields[13])
    stime = int(fields[14])
    ppid = int(fields[3])
    state = fields[2]
    num_threads = int(fields[19])
    rss_pages = int(fields[23])
    return dict(utime=utime, stime=stime, ppid=ppid, state=state,
                num_threads=num_threads, rss_pages=rss_pages, comm=comm)

def parse_status(pid):
    s = read_file(f'/proc/{pid}/status')
    out = {}
    if not s:
        return out
    for line in s.splitlines():
        if ':' not in line: continue
        k,v = line.split(':',1)
        out[k.strip()] = v.strip()
    return out

def parse_io(pid):
    s = read_file(f'/proc/{pid}/io')
    out = {}
    if not s:
        return out
    for line in s.splitlines():
        if ':' not in line: continue
        k,v = line.split(':',1)
        out[k.strip()] = v.strip()
    return out

def try_smaps_rollup(pid):
    s = read_file(f'/proc/{pid}/smaps_rollup')
    if not s:
        return None
    out = {}
    for line in s.splitlines():
        if ':' not in line: continue
        k,v = line.split(':',1)
        out[k.strip()] = v.strip()
    return out

def parse_smaps(pid):
    s = read_file(f'/proc/{pid}/smaps')
    if not s:
        return None
    anon = 0
    filebacked = 0
    cur_is_file = False
    for line in s.splitlines():
        m = re.match(r'^[0-9a-fA-F]+-[0-9a-fA-F]+', line)
        if m:
            parts = line.split()
            if len(parts) >= 6:
                pathname = parts[5]
                cur_is_file = not pathname.startswith('[')
            else:
                cur_is_file = False
            continue
        m = re.match(r'Rss:\s+(\d+)\s*kB', line)
        if m:
            val = int(m.group(1))
            if cur_is_file:
                filebacked += val
            else:
                anon += val
    return {'RssAnon_kB': anon, 'RssFile_kB': filebacked, 'Rss_total_kB': anon+filebacked}

def fmt(bytes_or_kb, kind='bytes'):
    if bytes_or_kb is None:
        return 'N/A'
    if kind == 'kB':
        b = int(bytes_or_kb) * 1024
    else:
        b = int(bytes_or_kb)
    if b >= 1024**3:
        return f'{b/1024**3:.2f} GiB'
    if b >= 1024**2:
        return f'{b/1024**2:.2f} MiB'
    if b >= 1024:
        return f'{b/1024:.1f} KiB'
    return f'{b} B'

# ---- helpers to run system commands for compare ----
def run_cmd(cmd):
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True, text=True)
        return out
    except subprocess.CalledProcessError as e:
        return f"(command failed) {e}\n{e.output}"

def cmd_exists(name):
    return shutil.which(name) is not None

def compare_with_tools(pid):
    blocks = []
    blocks.append(("ps", run_cmd(f"ps -p {pid} -o pid,ppid,comm,state,time,rss,vsz --no-headers")))
    blocks.append(("top", run_cmd(f"top -b -n1 -p {pid} 2>/dev/null | sed -n '7,9p'")))
    if cmd_exists("pidstat"):
        blocks.append(("pidstat", run_cmd(f"pidstat -p {pid} 1 1")))
    else:
        blocks.append(("pidstat", "(pidstat not found on system)"))
    blocks.append(("/proc/<pid>/stat", run_cmd(f"cat /proc/{pid}/stat 2>/dev/null || echo '(no access)'") ))
    blocks.append(("/proc/<pid>/status", run_cmd(f"grep -E 'VmRSS|VmSize|Threads|voluntary_ctxt_switches|nonvoluntary_ctxt_switches' /proc/{pid}/status 2>/dev/null || echo '(no access)'") ))
    blocks.append(("/proc/<pid>/io", run_cmd(f"cat /proc/{pid}/io 2>/dev/null || echo '(no access)'") ))
    return blocks

# ---- main printing ----
def print_summary(pid, do_compare=True):
    stat = parse_stat(pid)
    if not stat:
        print(f"Cannot read /proc/{pid}/stat")
        sys.exit(2)
    status = parse_status(pid)
    io = parse_io(pid)
    smap_roll = try_smaps_rollup(pid)
    smap = None
    if smap_roll:
        rss_kb = None
        rss_anon = None
        rss_file = None
        if 'Rss' in smap_roll:
            try:
                rss_kb = int(smap_roll['Rss'].split()[0])
            except:
                pass
        for key in ['Anon', 'AnonPages', 'RssAnon', 'AnonHugePages']:
            if key in smap_roll:
                try:
                    rss_anon = int(smap_roll[key].split()[0])
                except:
                    pass
        for key in ['File', 'RssFile']:
            if key in smap_roll:
                try:
                    rss_file = int(smap_roll[key].split()[0])
                except:
                    pass
        smap = {'Rss_kB': rss_kb, 'RssAnon_kB': rss_anon, 'RssFile_kB': rss_file}
    else:
        smap = parse_smaps(pid)

    clk_tck = os.sysconf('SC_CLK_TCK')
    page_size = os.sysconf('SC_PAGE_SIZE')
    cpu_time_sec = (stat['utime'] + stat['stime']) / clk_tck
    rss_bytes_from_stat = stat['rss_pages'] * page_size

    # Header / personalized touch
    print(f"pstat_personal v{__version__} — quick snapshot for PID {pid}\n")
    # system params
    print(f"system: CLK_TCK={clk_tck}  PAGE_SIZE={page_size} bytes")

    # compact summary (changed order / labels)
    print("--- process summary ---")
    print(f"PID/Name : {pid} / {stat['comm']}")
    print(f"PPid     : {stat['ppid']}    Threads: {stat['num_threads']}    State: {stat['state']}")
    print(f"CPU(ticks): utime={stat['utime']} stime={stat['stime']}   CPU seconds={(stat['utime']+stat['stime'])}/{clk_tck} = {cpu_time_sec:.3f}s")

    # memory
    vmrss = None
    vmsize = None
    if 'VmRSS' in status:
        m = re.match(r'(\d+)\s*kB', status['VmRSS'])
        if m:
            vmrss = int(m.group(1))
    if 'VmSize' in status:
        m2 = re.match(r'(\d+)\s*kB', status['VmSize'])
        if m2:
            vmsize = int(m2.group(1))

    print('\n--- memory ---')
    print(f"Memory RSS : {vmrss} kB ({fmt(vmrss,'kB')})")
    print(f"Stat RSS   : {rss_bytes_from_stat} bytes ({fmt(rss_bytes_from_stat,'bytes')})")
    if vmsize:
        try:
            perc = vmrss / vmsize * 100
            print(f"VmSize     : {vmsize} kB ({fmt(vmsize,'kB')})   RSS/VmSize = {perc:.1f}%")
        except Exception:
            pass

    if smap:
        print('\n--- smaps (rollup) ---')
        for k,v in smap.items():
            print(f"  {k}: {v}")
        if smap.get('Rss_kB'):
            print(f"  Total Rss (smaps_rollup): {smap['Rss_kB']} kB ({fmt(smap['Rss_kB'],'kB')})")

    # IO
    rb = io.get('read_bytes') if io else None
    wb = io.get('write_bytes') if io else None
    rchar = io.get('rchar') if io else None
    wchar = io.get('wchar') if io else None
    print('\n--- IO ---')
    print(f"rchar/wchar : {rchar} / {wchar}  (syscall bytes)")
    print(f"read_bytes/write_bytes: {rb} / {wb}  ({fmt(int(rb) if rb else 0,'bytes')} / {fmt(int(wb) if wb else 0,'bytes')})")

    # context switches
    vcs = status.get('voluntary_ctxt_switches', 'N/A')
    nvcs = status.get('nonvoluntary_ctxt_switches', 'N/A')
    print('\n--- context switches ---')
    print(f"voluntary   : {vcs}")
    print(f"non-voluntary: {nvcs}")

    # optional compare
    if do_compare:
        print('\n--- comparison with system tools ---\n')
        blocks = compare_with_tools(pid)
        for title, out in blocks:
            print(f"=== {title} ===")
            print(out)
            print('')


def main():
    parser = argparse.ArgumentParser(description="pstat_personal: read /proc/<pid> info (personal variant). By default runs comparisons.")
    parser.add_argument("pid", help="pid to inspect")
    parser.add_argument("--no-compare", action="store_true", help="don't run ps/top/pidstat comparisons (default: comparisons ON)")
    args = parser.parse_args()
    do_compare = not args.no_compare
    print_summary(args.pid, do_compare)

if __name__ == '__main__':
    main()

