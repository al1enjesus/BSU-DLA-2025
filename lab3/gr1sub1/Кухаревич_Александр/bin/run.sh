#!/bin/bash
set -e

echo "[*] Building pstat..."

if [ -z "$1" ]; then
    echo "Usage: ./run.sh <pid>"
    echo "Example: ./run.sh $$PPID"
    exit 1
fi

echo "[*] Running pstat for PID=$1"
./pstat "$1"
