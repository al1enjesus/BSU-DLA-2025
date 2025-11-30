#!/usr/bin/env python3
# src/worker.py
import os, time

BUF = b'0' * (1024*1024)  # 1 MiB

def write_sync(path, megabytes=50):
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
    # add O_SYNC to force synchronous writes (so the process will wait for writeback)
    if hasattr(os, "O_SYNC"):
        flags |= os.O_SYNC
    fd = os.open(path, flags, 0o644)
    try:
        written = 0
        for i in range(megabytes):
            n = os.write(fd, BUF)
            written += n
        # os.fsync redundant if O_SYNC used, but safe
        try:
            os.fsync(fd)
        except Exception:
            pass
    finally:
        os.close(fd)
    return written

def read_back(path):
    total = 0
    with open(path, 'rb') as f:
        while True:
            data = f.read(4*1024*1024)
            if not data:
                break
            total += len(data)
    return total

def main():
    print("worker: start")
    print("sleep 10s")
    time.sleep(10)

    path = os.path.join(os.getcwd(), "pstat_io_test.bin")
    print("writing sync to", path)
    written = write_sync(path, megabytes=50)
    print("written bytes:", written)

    print("reading back")
    readb = read_back(path)
    print("read bytes:", readb)

    print("sleep 30s")
    time.sleep(30)

    try:
        os.remove(path)
        print("removed", path)
    except Exception as e:
        print("cleanup failed:", e)

    print("worker: done")

if __name__ == "__main__":
    main()

