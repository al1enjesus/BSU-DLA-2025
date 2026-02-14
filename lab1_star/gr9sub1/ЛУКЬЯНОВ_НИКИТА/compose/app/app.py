import os
import sys
import time
import random

rate = int(os.getenv("RATE", "5"))            # messages per second
error_every = int(os.getenv("ERROR_EVERY", "10"))
duration = int(os.getenv("DURATION", "60"))    # seconds

start = time.time()
i = 0

def now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime())

while time.time() - start < duration:
    i += 1
    level = "INFO"
    msg = f"request handled ok {i}"
    if error_every > 0 and i % error_every == 0:
        level = random.choice(["ERROR", "FATAL"]) if random.random() < 0.3 else "ERROR"
        msg = random.choice([
            "db connection failed: timeout",
            "panic: nil pointer dereference",
            "fatal: cannot allocate resource",
            "error: upstream service unavailable",
        ])
    print(f"{level} {now_iso()} app: {msg}")
    sys.stdout.flush()
    time.sleep(max(0.0, 1.0 / max(rate, 1)))


