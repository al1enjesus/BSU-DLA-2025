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
        next(r)  # пропускаем заголовок
        for row in r:
            if len(row) < 5:
                continue
            mode, fs, operation, iter_no, time_sec = row
            try:
                t = float(time_sec) * 1000.0  # конвертируем секунды в миллисекунды
                data[(mode, operation)].append(t)
            except ValueError:
                continue

    # группируем по (mode, operation)
    keys = sorted(data.keys())
    labels = [f"{k[0]}\n{k[1]}" for k in keys]
    means = [sum(data[k]) / len(data[k]) for k in keys]
    errs = [np.std(data[k]) for k in keys]

    plt.figure(figsize=(max(8, len(keys) * 0.8), 6))
    x = np.arange(len(keys))
    plt.bar(x, means, yerr=errs, capsize=5, alpha=0.7)
    plt.xticks(x, labels, rotation=45, ha='right', fontsize=9)
    plt.ylabel('Latency (ms)')
    plt.title('Latency per operation and mode')
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = os.path.join(outdir, 'latency_summary.png')
    plt.savefig(out, dpi=150)
    print('Saved', out)
    plt.close()


def plot_throughput(csvfile, outdir):
    data = defaultdict(list)
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)  # пропускаем заголовок
        for row in r:
            if len(row) < 5:
                continue
            mode, fs, size_mb, secs, mb_per_s = row
            try:
                sz = float(size_mb)
                mb = float(mb_per_s)
                data[(mode, fs)].append((sz, mb))
            except ValueError:
                continue

    plt.figure(figsize=(10, 6))
    markers = ['o', 's', '^', 'v', 'D', 'p', '*']
    colors = ['blue', 'red', 'green', 'orange', 'purple', 'brown', 'pink']

    for idx, (key, lst) in enumerate(data.items()):
        lst.sort()
        xs = [s for s, _ in lst]
        ys = [y for _, y in lst]
        marker = markers[idx % len(markers)]
        color = colors[idx % len(colors)]
        plt.plot(xs, ys, marker=marker, label=f"{key[0]} {key[1]}",
                 linewidth=2, markersize=8, color=color)

    plt.xscale('log')
    plt.xlabel('Size (MB)')
    plt.ylabel('Throughput (MB/s)')
    plt.title('Throughput vs file size')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = os.path.join(outdir, 'throughput.png')
    plt.savefig(out, dpi=150)
    print('Saved', out)
    plt.close()


def plot_iops(csvfile, outdir):
    data = defaultdict(list)
    with open(csvfile) as f:
        r = csv.reader(f)
        next(r)  # пропускаем заголовок
        for row in r:
            if len(row) < 5:
                continue
            mode, fs, count, secs, files_per_s = row
            try:
                c = int(count)
                v = float(files_per_s)
                data[(mode, fs)].append((c, v))
            except ValueError:
                continue

    plt.figure(figsize=(10, 6))
    markers = ['o', 's', '^', 'v', 'D', 'p', '*']
    colors = ['blue', 'red', 'green', 'orange', 'purple', 'brown', 'pink']

    for idx, (key, lst) in enumerate(data.items()):
        lst.sort()
        xs = [c for c, _ in lst]
        ys = [v for _, v in lst]
        marker = markers[idx % len(markers)]
        color = colors[idx % len(colors)]
        plt.plot(xs, ys, marker=marker, label=f"{key[0]} {key[1]}",
                 linewidth=2, markersize=8, color=color)

    plt.xlabel('Number of files')
    plt.ylabel('Files/sec (IOPS)')
    plt.title('IOPS vs count')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = os.path.join(outdir, 'iops.png')
    plt.savefig(out, dpi=150)
    print('Saved', out)
    plt.close()


def usage():
    print('Usage: plot_results.py latency.csv throughput.csv iops.csv [outdir]')


if __name__ == '__main__':
    if len(sys.argv) < 4:
        usage();
        sys.exit(1)

    latency, throughput, iops = sys.argv[1:4]
    outdir = sys.argv[4] if len(sys.argv) > 4 else 'plots'

    # Проверка существования файлов
    for f in [latency, throughput, iops]:
        if not os.path.exists(f):
            print(f"Ошибка: файл не найден: {f}")
            sys.exit(1)

    os.makedirs(outdir, exist_ok=True)

    try:
        plot_latency(latency, outdir)
        plot_throughput(throughput, outdir)
        plot_iops(iops, outdir)
        print(f"\nГрафики успешно сохранены в директорию: {outdir}")
        print(f"1. {os.path.join(outdir, 'latency_summary.png')}")
        print(f"2. {os.path.join(outdir, 'throughput.png')}")
        print(f"3. {os.path.join(outdir, 'iops.png')}")
    except Exception as e:
        print(f"Ошибка при создании графиков: {e}")
        import traceback

        traceback.print_exc()