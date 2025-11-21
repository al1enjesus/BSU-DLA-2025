#!/bin/bash
# demo_mem.sh — демонстрация экспериментов по памяти (D)
# Запускайте из корня вашей папки (там где config.yaml и src/)
set -euo pipefail

SRC_DIR="$(dirname "$0")/src"
MEMTOUCH="$SRC_DIR/mem_touch.py"

if [ ! -f "$MEMTOUCH" ]; then
  echo "Не найден $MEMTOUCH"
  exit 1
fi

# 1) Запустить mem_touch обычным режимом
echo "=== 1) Запуск mem_touch (128MB target, шаг 32MB) ==="
python3 "$MEMTOUCH" --rss-mb 128 --step-mb 32 --sleep-ms 200 &
PID=$!
echo "mem_touch pid = $PID"
sleep 1

echo
echo "free -h:"
free -h
echo

echo "pidstat -r -p $PID 1 5 & (выводит mem stats для процесса)"
pidstat -r -p $PID 1 5 &

# параллельно печатаем /proc/<pid>/status 1 раз
sleep 1
echo "cat /proc/$PID/status | head -20"
cat /proc/$PID/status | head -20

echo
echo "Нажмите Enter чтобы продолжить к эксперименту с ulimit (или Ctrl-C чтобы прервать)"
read -r

# 2) ulimit -v (виртуальная память) — запустим новую копию в ограниченном шелле
echo "=== 2) Тест ulimit -v: ограничим виртуальную память до 200MB и запустим mem_touch ==="
# limit в KB
LIMIT_KB=$((200*1024))
bash -c "ulimit -v $LIMIT_KB; echo 'ulimit -v set to' $(ulimit -v); python3 $MEMTOUCH --rss-mb 256 --step-mb 64 --sleep-ms 200" &
PID_ULIM=$!
echo "pid (ulimit run) = $PID_ULIM"
sleep 2
echo "cat /proc/$PID_ULIM/status | head -20" || true
cat /proc/$PID_ULIM/status | head -20 || true

echo "Подождите 6 секунд для наблюдений..."
sleep 6

echo
echo "Остановим демо (убьём все запущенные mem_touch)..."
kill -TERM $PID || true
kill -TERM $PID_ULIM || true
wait $PID 2>/dev/null || true
wait $PID_ULIM 2>/dev/null || true

echo "=== 3) Тест setrlimit внутри скрипта: запустим mem_touch с --limit-mb 200 ==="
python3 "$MEMTOUCH" --rss-mb 256 --step-mb 64 --sleep-ms 200 --limit-mb 200 &
PID_RL=$!
echo "pid (setrlimit run) = $PID_RL"
sleep 3
echo "cat /proc/$PID_RL/status | head -20" || true
cat /proc/$PID_RL/status | head -20 || true
sleep 4
kill -TERM $PID_RL 2>/dev/null || true
wait $PID_RL 2>/dev/null || true

echo
echo "=== 4) (опционально) cgroup v2 demo ==="
echo "Если вы хотите попробовать cgroup v2, см. README ниже — требуется root и наличие монтированного cgroup v2."
echo "Готово. Конец демо."

