!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
make
./supervisor ./config.cfg &
SUP_PID=$!
echo "Super pid=$SUP_PID"
sleep 1
echo ""
echo "Children of supervisor:"
ps -o pid,ppid,cmd --ppid $SUP_PID

echo ""
echo "Switch to LIGHT (SIGUSR1)"
kill -USR1 $SUP_PID
sleep 2

echo ""
echo "Switch to HEAVY (SIGUSR2)"
kill -USR2 $SUP_PID
sleep 2

echo ""
echo "Simulate crash of one worker (kill -9)"
CHILD=$(ps --no-headers -o pid --ppid $SUP_PID | head -n1)
echo "Killing child pid=$CHILD"
kill -9 $CHILD
sleep 2

echo ""
echo "Reload config (SIGHUP)"
kill -HUP $SUP_PID
sleep 2

echo ""
echo "Graceful shutdown (SIGTERM)"
kill -TERM $SUP_PID
wait $SUP_PID || true
echo "Supervisor exited"