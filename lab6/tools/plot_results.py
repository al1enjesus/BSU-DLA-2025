#!/usr/bin/env python3
"""
Plot results from CSVs produced by bench_full.sh

Usage:
  python3 plot_results.py latency.csv throughput.csv iops.csv [outdir]

Produces PNG files: latency_summary.png, throughput.png, iops.png
"""
import sys
import csv
import os
from collections import defaultdict
import math

try:
    import matplotlib.pyplot as plt
    import numpy as np
except Exception as e:
    print("matplotlib/numpy required to plot. Install and retry.")
    raise

def plot_latency(csvfile, outdir):
    data = defaultdict(list)
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,op,iter_no,t in r:
            t = float(t)*1000.0
            data[(mode,op)].append(t)

    # group by (mode, op)
    keys = sorted(data.keys())
    labels = [f"{k[0]}\n{k[1]}" for k in keys]
    means = [sum(data[k])/len(data[k]) for k in keys]
    errs = [np.std(data[k]) for k in keys]

    plt.figure(figsize=(max(8,len(keys)*0.6),5))
    x = np.arange(len(keys))
    plt.bar(x, means, yerr=errs, capsize=3)
    plt.xticks(x, labels, rotation=45, ha='right')
    plt.ylabel('Latency (ms)')
    plt.title('Latency per operation and mode')
    plt.tight_layout()
    out = os.path.join(outdir, 'latency_summary.png')
    plt.savefig(out)
    print('Saved', out)

def plot_throughput(csvfile, outdir):
    data = defaultdict(list)
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,size,secs,mbs in r:
            try:
                sz = float(size)
                mb = float(mbs)
            except:
                continue
            data[(mode,fs)].append((sz,mb))

    plt.figure()
    for key, lst in data.items():
        lst.sort()
        xs = [s for s,_ in lst]
        ys = [y for _,y in lst]
        plt.plot(xs, ys, marker='o', label=f"{key[0]} {key[1]}")
    plt.xscale('log')
    plt.xlabel('Size (MB)')
    plt.ylabel('Throughput (MB/s)')
    plt.title('Throughput vs file size')
    plt.legend()
    plt.tight_layout()
    out = os.path.join(outdir, 'throughput.png')
    plt.savefig(out)
    print('Saved', out)

def plot_iops(csvfile, outdir):
    data = defaultdict(list)
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,count,secs,fps in r:
            try:
                c=int(count)
                v=float(fps)
            except:
                continue
            data[(mode,fs)].append((c,v))

    plt.figure()
    for key, lst in data.items():
        lst.sort()
        xs = [c for c,_ in lst]
        ys = [v for _,v in lst]
        plt.plot(xs, ys, marker='o', label=f"{key[0]} {key[1]}")
    plt.xlabel('Number of files')
    plt.ylabel('Files/sec (IOPS)')
    plt.title('IOPS vs count')
    plt.legend()
    plt.tight_layout()
    out = os.path.join(outdir, 'iops.png')
    plt.savefig(out)
    print('Saved', out)

def usage():
    print('Usage: plot_results.py latency.csv throughput.csv iops.csv [outdir]')

if __name__ == '__main__':
    if len(sys.argv) < 4:
        usage(); sys.exit(1)
    latency, throughput, iops = sys.argv[1:4]
    outdir = sys.argv[4] if len(sys.argv) > 4 else os.path.dirname(latency) or '.'
    os.makedirs(outdir, exist_ok=True)
    plot_latency(latency, outdir)
    plot_throughput(throughput, outdir)
    plot_iops(iops, outdir)
