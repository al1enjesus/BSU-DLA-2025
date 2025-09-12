#!/usr/bin/env python3
import argparse
import json
import random
from datetime import datetime, timedelta, timezone
from pathlib import Path


def iso_nano(dt: datetime, extra_nanos: int) -> str:
    # Python supports microseconds only; synthesize 9-digit nanos
    total_nanos = dt.microsecond * 1000 + (extra_nanos % 1000)
    nanos_str = f"{total_nanos:09d}"
    base = dt.replace(microsecond=0).strftime("%Y-%m-%dT%H:%M:%S")
    return f"{base}.{nanos_str}Z"


def gen_nginx_line(i: int, base_dt: datetime) -> dict:
    # Alternate between ok, not found, and error bursts
    status = random.choices([200, 404, 500], weights=[80, 15, 5])[0]
    path = random.choice(["/", "/api", "/health", "/static/logo.png", "/login", "/notfound"]) 
    ua = random.choice(["curl/8.4.0", "Mozilla/5.0", "k6/0.50", "Go-http-client/1.1"]) 
    ip = f"172.17.0.{random.randint(1, 254)}"
    dt = base_dt + timedelta(seconds=i // 3)
    t = iso_nano(dt, random.randint(0, 999))

    # stderr error message occasionally
    if status == 500 and random.random() < 0.7:
        msg = (
            "[error] 12#12: *1 upstream timed out (110: Connection timed out) "
            f"while reading response header from upstream, client: {ip}, server: _, "
            f"request: \"GET {path} HTTP/1.1\", upstream: \"http://172.17.0.2:8080{path}\", host: \"localhost\""
        )
        return {"log": msg + "\n", "stream": "stderr", "time": t}
    # access log stdout
    size = random.randint(0, 2048)
    msg = f"{ip} - - [{dt.strftime('%Y/%m/%d:%H:%M:%S +0000')}] \"GET {path} HTTP/1.1\" {status} {size} \"-\" \"{ua}\""
    return {"log": msg + "\n", "stream": "stdout", "time": t}


def gen_app_line(i: int, base_dt: datetime) -> dict:
    dt = base_dt + timedelta(seconds=i // 2)
    t = iso_nano(dt, random.randint(0, 999))
    if i % 37 == 0:
        level = random.choice(["ERROR", "FATAL"]) 
        msg = random.choice([
            "db connection failed: timeout",
            "panic: nil pointer dereference",
            "fatal: cannot allocate resource",
            "error: upstream service unavailable",
        ])
        return {"log": f"{level} {msg}\n", "stream": "stderr", "time": t}
    if i % 113 == 0:
        return {"log": "WARN slow query detected\n", "stream": "stdout", "time": t}
    return {"log": f"INFO request handled ok {i}\n", "stream": "stdout", "time": t}


def write_json_lines(path: Path, lines):
    with path.open("w", encoding="utf-8") as f:
        for obj in lines:
            f.write(json.dumps(obj, ensure_ascii=False))
            f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lines", type=int, default=1200, help="lines per fixture file")
    args = ap.parse_args()

    fixtures_dir = Path(__file__).parent
    nginx_path = fixtures_dir / "nginx-json.log"
    app_path = fixtures_dir / "app-json.log"

    base_dt = datetime(2025, 9, 12, 0, 0, 0, tzinfo=timezone.utc)

    nginx_lines = [gen_nginx_line(i, base_dt) for i in range(args.lines)]
    app_lines = [gen_app_line(i, base_dt) for i in range(args.lines)]

    write_json_lines(nginx_path, nginx_lines)
    write_json_lines(app_path, app_lines)

    print(f"Wrote {len(nginx_lines)} lines to {nginx_path}")
    print(f"Wrote {len(app_lines)} lines to {app_path}")


if __name__ == "__main__":
    main()


