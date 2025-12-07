#!/usr/bin/env python3
"""
ascii_graph.py

Парсит CSV (test,fs,value,unit) и печатает ASCII-графики по группам тестов.
Usage:
    python3 ascii_graph.py results.csv
"""

import sys
import csv
from collections import defaultdict, OrderedDict

if len(sys.argv) < 2:
    print("Usage: python3 ascii_graph.py <results.csv>")
    sys.exit(1)

csvfile = sys.argv[1]

# Load CSV
data = defaultdict(dict)  # data[test][fs] = (value, unit)
order = []  # preserve test order
units = {}

with open(csvfile, newline='') as f:
    reader = csv.DictReader(f)
    for row in reader:
        test = row['test']
        fs = row['fs']
        val = float(row['value'])
        unit = row.get('unit','')
        data[test][fs] = val
        units[test] = unit
        if test not in order:
            order.append(test)

# For nicer ordering, group latency tests first
preferred_order = []
for t in order:
    if t in ('open','getattr','read_4k','write_4k'):
        preferred_order.append(t)
# then throughput read/write pairs
for size in (1,10,100):
    t1 = f"throughput_write_{size}MB"
    t2 = f"throughput_read_{size}MB"
    if t1 in data and t1 not in preferred_order: preferred_order.append(t1)
    if t2 in data and t2 not in preferred_order: preferred_order.append(t2)
# remaining
for t in order:
    if t not in preferred_order:
        preferred_order.append(t)

# Determine max value among all tests for scaling bars
max_vals = {}
for t, vals in data.items():
    max_vals[t] = max(vals.values()) if vals else 0.0

# Utility to draw bar
def draw_bar(value, maxval, max_chars=5):
    if maxval <= 0:
        return ""
    length = int(round((value / maxval) * max_chars))
    if length < 1 and value > 0:
        length = 1
    return "▉" * length

# Print grouped ASCII graphs
print("\n**ASCII graphs**\n")
# If latency tests present, print human-friendly names
nice_names = {
    'open': 'open    ',
    'getattr': 'getattr ',
    'read_4k': 'read 4K ',
    'write_4k': 'write4K '
}

for test in preferred_order:
    vals = data.get(test)
    if not vals:
        continue
    unit = units.get(test,"")
    # Prepare label
    label = nice_names.get(test, test)
    # Use per-test max for scaling
    m = max_vals[test]
    # Build line for ext4 and fuse
    left = vals.get('ext4', 0.0)
    right = vals.get('fuse', 0.0)
    # For latency (ms) smaller is better; but bars are proportional — we still show numbers.
    bar_ext4 = draw_bar(left, max(left,right) if max(left,right)>0 else 1)
    bar_fuse = draw_bar(right, max(left,right) if max(left,right)>0 else 1)
    print(f"{label} | ext4: {bar_ext4} {left} {unit}  FUSE: {bar_fuse} {right} {unit}")

# Also print throughput and IOPS grouped more readably
# Collect groups
print("\nSummary table (test, ext4, fuse, unit):\n")
print(f"{'test':30s} {'ext4':>12s} {'fuse':>12s} {'unit':>8s}")
print("-"*68)
for test in preferred_order:
    vals = data.get(test)
    if not vals: continue
    ext4 = vals.get('ext4', 0.0)
    fuse = vals.get('fuse', 0.0)
    unit = units.get(test,"")
    print(f"{test:30s} {ext4:12.6f} {fuse:12.6f} {unit:>8s}")

print("\nNote: Bars are scaled per-test (max value among compared FS).")
