cd src
python3 supervisor.py ../config.json &
SUP_PID=$!
sleep 3

echo "Switching to light mode"
kill -USR1 $SUP_PID
sleep 5

echo "Switching back to heavy mode"
kill -USR2 $SUP_PID
sleep 5

echo "Reloading config"
kill -HUP $SUP_PID
sleep 5

echo "Graceful shutdown"
if kill -0 $SUP_PID 2>/dev/null; then
  kill -TERM $SUP_PID
else
  echo "Supervisor already exited."
fi

pkill -f "python3 worker.py"

