import matplotlib.pyplot as plt
import csv
import numpy as np
import collections

# Файл данных
CSV_FILE = 'results.csv'

# Структура: data[Category][Parameter][Filesystem] = Seconds
data = collections.defaultdict(lambda: collections.defaultdict(dict))

def load_data():
    try:
        with open(CSV_FILE, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    cat = row['Category']
                    param = row['Parameter']
                    fs = row['Filesystem']
                    val = float(row['Value'])
                    data[cat][param][fs] = val
                except ValueError:
                    continue
    except FileNotFoundError:
        print("Файл results.csv не найден!")
        exit(1)

def plot_latency():
    # График 1: Latency (ms) vs Тип операции
    cat = "Latency"
    ops = ['open', 'read', 'write', 'getattr']
    
    # Переводим секунды в миллисекунды (* 1000)
    native = [data[cat][op].get('Native', 0) * 1000 for op in ops]
    fuse =   [data[cat][op].get('FUSE', 0)   * 1000 for op in ops]

    x = np.arange(len(ops))
    width = 0.35

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.bar(x - width/2, native, width, label='Native FS', color='skyblue')
    ax.bar(x + width/2, fuse, width, label='FUSE FS', color='salmon')

    ax.set_ylabel('Latency (ms) - Lower is Better')
    ax.set_title('Latency vs Operation Type')
    ax.set_xticks(x)
    ax.set_xticklabels(ops)
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.savefig('graph_1_latency.png')
    print("Graph 1 saved.")

def plot_throughput():
    # График 2: Throughput (MB/s) vs Размер файла
    cat = "Throughput"
    sizes = ['1', '10', '100'] # MB
    
    # Считаем скорость: Size (MB) / Time (s) = MB/s
    native = []
    fuse = []
    
    for s in sizes:
        t_nat = data[cat][s].get('Native', 0.001)
        t_fus = data[cat][s].get('FUSE', 0.001)
        native.append(float(s) / t_nat if t_nat > 0 else 0)
        fuse.append(float(s) / t_fus if t_fus > 0 else 0)

    x = np.arange(len(sizes))
    width = 0.35

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.bar(x - width/2, native, width, label='Native FS', color='#1f77b4')
    ax.bar(x + width/2, fuse, width, label='FUSE FS', color='#ff7f0e')

    ax.set_ylabel('Throughput (MB/s) - Higher is Better')
    ax.set_xlabel('File Size (MB)')
    ax.set_title('Write Throughput vs File Size')
    ax.set_xticks(x)
    ax.set_xticklabels([f"{s}MB" for s in sizes])
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.savefig('graph_2_throughput.png')
    print("Graph 2 saved.")

def plot_iops():
    # График 3: IOPS vs Количество файлов
    cat = "IOPS"
    counts = ['1000']
    
    # Считаем IOPS: Count / Time (s)
    native = []
    fuse = []
    
    for c in counts:
        t_nat = data[cat][c].get('Native', 0.001)
        t_fus = data[cat][c].get('FUSE', 0.001)
        native.append(float(c) / t_nat if t_nat > 0 else 0)
        fuse.append(float(c) / t_fus if t_fus > 0 else 0)

    x = np.arange(len(counts))
    width = 0.35

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.bar(x - width/2, native, width, label='Native FS', color='lightgreen')
    ax.bar(x + width/2, fuse, width, label='FUSE FS', color='orange')

    ax.set_ylabel('IOPS (Ops/sec) - Higher is Better')
    ax.set_xlabel('Number of Files Created')
    ax.set_title('IOPS vs File Count')
    ax.set_xticks(x)
    ax.set_xticklabels(counts)
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.savefig('graph_3_iops.png')
    print("Graph 3 saved.")

if __name__ == "__main__":
    load_data()
    plot_latency()
    plot_throughput()
    plot_iops()
