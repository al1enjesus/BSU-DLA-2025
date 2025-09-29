# src/logger.py
import os
from datetime import datetime

class Logger:
    """
    Simple process-safe logger with fixed column output:
    TS | COMPONENT | PID | LEVEL | MESSAGE | EXTRA
    Columns are separated by a single tab character for easy parsing.
    """
    def __init__(self, path: str):
        self.path = path
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        self.fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o644)

    def _write_line(self, text: str):
        try:
            os.write(self.fd, (text + "\n").encode())
            print(text)
        except Exception:
            pass  # ignore logging failures

    def log(self, level: str, component: str, message: str, **extra):
        ts = datetime.utcnow().isoformat() + "Z"
        pid = os.getpid()
        # convert extra dict to key=value pairs separated by commas
        extra_str = ",".join(f"{k}={v}" for k, v in extra.items())
        # columns separated by tabs
        line = f"{ts}\t{component}\t{pid}\t{level}\t{message}\t{extra_str}"
        self._write_line(line)

    def info(self, component: str, message: str, **extra):
        self.log("INFO", component, message, **extra)

    def error(self, component: str, message: str, **extra):
        self.log("ERROR", component, message, **extra)

    def close(self):
        try:
            os.close(self.fd)
        except Exception:
            pass
