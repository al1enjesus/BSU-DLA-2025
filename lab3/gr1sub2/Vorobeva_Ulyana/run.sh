#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/src/pstat.py"

# Проверка прав на выполнение Python скрипта
if [ ! -x "$SRC" ]; then
    chmod +x "$SRC" || true
fi

# --- PID валидация ---
if [ $# -eq 0 ]; then
    PID=$$
else
    PID=$1
fi

if ! [[ "$PID" =~ ^[0-9]+$ ]]; then
    echo "Ошибка: PID должен быть числом, получено '$PID'"
    exit 1
fi

if ! ps -p "$PID" > /dev/null 2>&1; then
    echo "Процесс с PID $PID не найден."
    exit 1
fi

echo "Running pstat for PID $PID"
python3 "$SRC" "$PID"

# --- Сравнение с системными утилитами ---
echo -e "\n--- ps output ---"
ps -p "$PID" -o pid,ppid,state,utime,stime,rss,nlwp,comm 2>/dev/null || true

echo -e "\n--- top (one snapshot) ---"
top -b -n 1 -p "$PID" 2>/dev/null | sed -n '1,20p'

if command -v pidstat >/dev/null 2>&1; then
    echo -e "\n--- pidstat (1s sample) ---"
    pidstat -p "$PID" 1 1
fi

# --- strace с безопасным временным файлом ---
if command -v strace >/dev/null 2>&1; then
    echo -e "\n--- strace -c (short 2s attach; may require root) ---"
    if [ "$PID" -ne "$$" ]; then
        TMP_FILE=$(mktemp /tmp/strace_pstat.XXXXXX)
        if [ "$(id -u)" -eq 0 ]; then
            sudo strace -f -c -p "$PID" -o "$TMP_FILE" -q -s 64 &
            STRACE_PID=$!
            sleep 2
            sudo kill -INT "$STRACE_PID" 2>/dev/null || true
            echo "strace summary (if produced):"
            sudo cat "$TMP_FILE" || true
        else
            echo "strace требует root или права sudo"
        fi
        rm -f "$TMP_FILE"
    else
        echo "Skipping strace on current shell to avoid noise"
    fi
fi

# --- perf с проверкой прав ---
if command -v perf >/dev/null 2>&1; then
    echo -e "\n--- perf stat (1s) ---"
    if [ "$(id -u)" -eq 0 ]; then
        sudo perf stat -p "$PID" sleep 1 2>/dev/null || true
    else
        echo "perf требует root или права sudo"
    fi
fi
