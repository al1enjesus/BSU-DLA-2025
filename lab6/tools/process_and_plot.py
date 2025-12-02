#!/usr/bin/env python3
import matplotlib.pyplot as plt
import csv
import sys
from statistics import mean, median, stdev

def plot_latency(csvfile):
    data = {}
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,op,iter_no,t in r:
            t = float(t)
            key = (mode, fs, op)
            data.setdefault(key, []).append(t*1000.0) # ms

    # aggregate by (mode, op) averaging across fs
    ops = sorted(list({k[2] for k in data.keys()}))
    modes = sorted(list({k[0] for k in data.keys()}))

    fig, ax = plt.subplots()
    labels = []
    vals = []
    for key in sorted(data.keys()):
        mode, fs, op = key
        labels.append(f"{mode}\n{fs}\n{op}")
        vals.append(mean(data[key]))

    ax.bar(range(len(vals)), vals)
    ax.set_xticks(range(len(vals)))
    ax.set_xticklabels(labels, rotation=45, ha='right')
    ax.set_ylabel('latency (ms)')
    plt.tight_layout()
    plt.savefig('latency_summary.png')

def plot_throughput(csvfile):
    # CSV: mode,fs,size_mb,secs,mb_per_s
    sizes = {}
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,size,secs,mbs in r:
            key = (mode, fs)
            sizes.setdefault(key, []).append((int(size), float(mbs)))

    fig, ax = plt.subplots()
    for key, lst in sizes.items():
        lst.sort()
        xs = [s for s,_ in lst]
        ys = [b for _,b in lst]
        ax.plot(xs, ys, marker='o', label=f"{key[0]} {key[1]}")
    ax.set_xscale('log')
    ax.set_xlabel('size MB')
    ax.set_ylabel('MB/s')
    ax.legend()
    plt.tight_layout()
    plt.savefig('throughput.png')

def plot_iops(csvfile):
    # CSV: mode,fs,count,secs,files_per_s
    data = {}
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)
        for mode,fs,count,secs,fps in r:
            data.setdefault((mode,fs), []).append((int(count), float(fps)))

    fig, ax = plt.subplots()
    for key, lst in data.items():
        lst.sort()
        xs = [c for c,_ in lst]
        ys = [v for _,v in lst]
        ax.plot(xs, ys, marker='o', label=f"{key[0]} {key[1]}")
    ax.set_xlabel('num files')
    ax.set_ylabel('files/sec')
    ax.legend()
    plt.tight_layout()
    plt.savefig('iops.png')

def usage():
    print('process_and_plot.py <latency.csv> <throughput.csv> <iops.csv>')

if __name__ == '__main__':
    if len(sys.argv) < 4:
        usage(); sys.exit(1)
    plot_latency(sys.argv[1])
    plot_throughput(sys.argv[2])
    plot_iops(sys.argv[3])
    print('Saved latency_summary.png, throughput.png, iops.png')
