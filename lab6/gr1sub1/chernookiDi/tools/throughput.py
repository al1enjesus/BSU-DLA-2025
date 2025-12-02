#!/usr/bin/env python3
import sys
import time

def measure_read(path):
    buf_size = 1024*1024
    total = 0
    t0 = time.perf_counter()
    with open(path, 'rb') as f:
        while True:
            data = f.read(buf_size)
            if not data:
                break
            total += len(data)
    t1 = time.perf_counter()
    secs = t1 - t0
    mb = total / (1024*1024)
    print(f"{mb:.3f},{secs:.6f},{mb/secs:.3f}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('usage: throughput.py <file>')
        sys.exit(1)
    measure_read(sys.argv[1])
