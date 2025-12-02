#!/usr/bin/env python3
# Simple plotting script for CSV results
import matplotlib.pyplot as plt
import csv
import sys

if len(sys.argv) < 2:
    print("Usage: plot_stats.py results.csv")
    sys.exit(1)

csvf = sys.argv[1]
xs = []
ys = []
labels = []
with open(csvf) as f:
    r = csv.reader(f)
    next(r)
    for row in r:
        test, fs, bytes_s, time_s = row
        labels.append(test)
        xs.append(int(bytes_s) if bytes_s.isdigit() else 0)
        ys.append(float(time_s))

plt.figure()
plt.bar(labels, ys)
plt.xticks(rotation=45, ha='right')
plt.ylabel('time (s)')
plt.tight_layout()
plt.show()
