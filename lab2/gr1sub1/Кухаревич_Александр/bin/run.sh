set -e

./supervisor config.cfg &
SUP=$!
sleep 1

ps -o pid,ppid,ni,cmd -u $USER | grep worker || true

pidstat -u 1 5

kill -USR1 $SUP; sleep 2

kill -USR2 $SUP; sleep 2

kill -TERM $SUP
wait $SUP || true
