import matplotlib.pyplot as plt
import numpy as np

# ------------------------------
# Latency graph
# ------------------------------
ops = ["open", "read 4KB", "write 4KB", "getattr"]
apfs = [0.05, 0.07, 0.08, 0.04]
fuse = [0.10, 0.12, 0.14, 0.09]

x = np.arange(len(ops))
plt.figure(figsize=(8,5))
plt.bar(x - 0.15, apfs, width=0.3, label="APFS")
plt.bar(x + 0.15, fuse, width=0.3, label="FUSE")
plt.xticks(x, ops)
plt.ylabel("Latency (ms)")
plt.title("Latency Comparison: APFS vs FUSE (MacBook Pro M1)")
plt.legend()
plt.grid(axis="y", alpha=0.3)
plt.savefig("latency.png", dpi=200)

# ------------------------------
# Throughput graph
# ------------------------------
sizes = [1, 10, 100]
apfs_t = [865, 900, 910]
fuse_t = [520, 515, 500]

plt.figure(figsize=(8,5))
plt.plot(sizes, apfs_t, marker="o", label="APFS", linewidth=2)
plt.plot(sizes, fuse_t, marker="o", label="FUSE", linewidth=2)
plt.xlabel("File size (MB)")
plt.ylabel("Throughput (MB/s)")
plt.title("Throughput Test (APFS vs FUSE)")
plt.grid(True, alpha=0.3)
plt.legend()
plt.savefig("throughput.png", dpi=200)

# ------------------------------
# IOPS graph
# ------------------------------
file_counts = [100, 300, 500, 1000]
apfs_iops = [0.03, 0.09, 0.15, 0.29]
fuse_iops = [0.07, 0.19, 0.31, 0.59]

plt.figure(figsize=(8,5))
plt.plot(file_counts, apfs_iops, marker="s", label="APFS", linewidth=2)
plt.plot(file_counts, fuse_iops, marker="s", label="FUSE", linewidth=2)
plt.xlabel("Number of small files (1KB)")
plt.ylabel("Time (s)")
plt.title("IOPS Test (APFS vs FUSE)")
plt.grid(True, alpha=0.3)
plt.legend()
plt.savefig("iops.png", dpi=200)

print("Графики созданы: latency.png, throughput.png, iops.png")
