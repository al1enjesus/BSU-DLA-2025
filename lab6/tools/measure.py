#!/usr/bin/env python3
import argparse
import os
import time

def time_open_close(path):
    t0 = time.perf_counter_ns()
    fd = os.open(path, os.O_RDONLY)
    os.close(fd)
    t1 = time.perf_counter_ns()
    return (t1 - t0) / 1e9

def time_read(path, size):
    fd = os.open(path, os.O_RDONLY)
    t0 = time.perf_counter_ns()
    os.read(fd, size)
    t1 = time.perf_counter_ns()
    os.close(fd)
    return (t1 - t0) / 1e9

def time_write(path, size):
    # open for write, write size bytes at offset 0
    data = b"X" * size
    fd = os.open(path, os.O_WRONLY)
    t0 = time.perf_counter_ns()
    os.pwrite(fd, data, 0)
    t1 = time.perf_counter_ns()
    os.close(fd)
    return (t1 - t0) / 1e9

def time_getattr(path):
    t0 = time.perf_counter_ns()
    os.stat(path)
    t1 = time.perf_counter_ns()
    return (t1 - t0) / 1e9

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--operation', required=True, choices=['open','read','write','getattr'])
    p.add_argument('--file', required=True)
    p.add_argument('--iterations', type=int, default=1000)
    p.add_argument('--size', type=int, default=4096)
    args = p.parse_args()

    # ensure file exists
    if not os.path.exists(args.file):
        # create file of at least size
        with open(args.file, 'wb') as f:
            f.write(b'0' * max(1, args.size))

    for i in range(args.iterations):
        try:
            if args.operation == 'open':
                t = time_open_close(args.file)
            elif args.operation == 'read':
                t = time_read(args.file, args.size)
            elif args.operation == 'write':
                t = time_write(args.file, args.size)
            elif args.operation == 'getattr':
                t = time_getattr(args.file)
        except Exception as e:
            print('ERROR', e)
            return
        print(f"{t:.9f}")

if __name__ == '__main__':
    main()
