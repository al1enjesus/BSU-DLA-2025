import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys

# Настройка стиля
sns.set_theme(style="whitegrid")
FILE_NAME = "results.csv"

try:
    df = pd.read_csv(FILE_NAME)
except FileNotFoundError:
    print(f"Error: {FILE_NAME} not found. Run benchmark.sh first.")
    sys.exit(1)

# Создаем фигуру с 3 графиками (1 строка, 3 колонки)
fig, axes = plt.subplots(1, 3, figsize=(18, 6))
fig.suptitle('FUSE Filesystem Performance Analysis', fontsize=16)

# --- График 1: Latency (Меньше = Лучше) ---
# Фильтруем данные
latency_df = df[df['TestType'] == 'Latency']
sns.barplot(ax=axes[0], data=latency_df, x='Operation', y='Value', hue='FileSystem', palette="muted")
axes[0].set_title('Latency (Lower is Better)')
axes[0].set_ylabel('Time (ms)')
axes[0].set_xlabel('Operation')

# Добавляем значения на столбцы
for container in axes[0].containers:
    axes[0].bar_label(container, fmt='%.2f')

# --- График 2: Throughput (Больше = Лучше) ---
throughput_df = df[df['TestType'] == 'Throughput']
sns.barplot(ax=axes[1], data=throughput_df, x='Operation', y='Value', hue='FileSystem', palette="viridis")
axes[1].set_title('Throughput (Higher is Better)')
axes[1].set_ylabel('Speed (MB/s)')
axes[1].set_xlabel('Operation')

for container in axes[1].containers:
    axes[1].bar_label(container, fmt='%.0f')

# --- График 3: IOPS (Больше = Лучше) ---
iops_df = df[df['TestType'] == 'IOPS']
sns.barplot(ax=axes[2], data=iops_df, x='Operation', y='Value', hue='FileSystem', palette="magma")
axes[2].set_title('IOPS (Higher is Better)')
axes[2].set_ylabel('Operations per Second')
axes[2].set_xlabel('Small File Creation')

for container in axes[2].containers:
    axes[2].bar_label(container, fmt='%.0f')

plt.tight_layout()
plt.savefig('benchmark_report.png')
print("Graphs saved to 'benchmark_report.png'")
plt.show()

