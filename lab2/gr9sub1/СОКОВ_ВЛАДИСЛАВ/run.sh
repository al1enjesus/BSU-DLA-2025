#!/bin/bash

cleanup() {
    if [ -n "$SUPERVISOR_PID" ] && kill -0 "$SUPERVISOR_PID" 2>/dev/null; then
        echo ""
        echo ">>> Stopping supervisor..."
        kill -SIGTERM $SUPERVISOR_PID 2>/dev/null
        wait $SUPERVISOR_PID 2>/dev/null
    fi
    echo ">>> Done."
}

trap cleanup EXIT INT TERM

echo ">>> Building sources..."
cd src
make clean
make
cd .. || exit 1

echo ">>> Starting Supervisor in background..."
./src/supervisor &
SUPERVISOR_PID=$!

echo ">>> Supervisor PID: $SUPERVISOR_PID"
echo ">>> Wait 3 seconds for workers to stabilize..."
sleep 3

echo ">>> Current process tree:"
ps --forest -o pid,ppid,comm,stat -g $SUPERVISOR_PID

echo "-----------------------------------------------------"
echo "COMMANDS TO TRY (Copy and Paste):"
echo "1. Switch to LIGHT mode:  kill -SIGUSR1 $SUPERVISOR_PID"
echo "2. Switch to HEAVY mode:  kill -SIGUSR2 $SUPERVISOR_PID"
echo "3. Reload config (edit src/config.ini first): kill -SIGHUP $SUPERVISOR_PID"
echo "4. Kill a worker (test restart): kill <WORKER_PID>"
echo "5. Stop everything:       kill -SIGTERM $SUPERVISOR_PID"
echo "-----------------------------------------------------"

# Ожидаем Ctrl + C или сигнала SIGINT/SIGTERM
read -p "Press [Ctrl + C] to stop the demo and kill supervisor..." || true

wait