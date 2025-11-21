#!/usr/bin/env bash
# bench_utils.sh - вспомогательные функции для бенчмарка
# Поместите рядом с run_bench.sh и ascii_graph.py

set -euo pipefail

# Получить текущее время в миллисекундах (GNU date)
now_ms() {
  date +%s%3N
}

# Возвращает время выполнения команды в миллисекундах
# usage: measure_ms <cmd...>
measure_ms() {
  local start end rc
  start=$(now_ms)
  "$@"
  rc=$?
  end=$(now_ms)
  printf "%d\n" $((end - start))
  return $rc
}

# Измеряет время выполнения команды с повторениями (для микросекундных операций)
# usage: measure_ms_repeated <iterations> <cmd...>
# Возвращает среднее время в миллисекундах
measure_ms_repeated() {
  local iterations=$1
  shift
  local start end total_ms
  start=$(now_ms)
  for i in $(seq 1 $iterations); do
    "$@" >/dev/null 2>&1
  done
  end=$(now_ms)
  total_ms=$((end - start))
  # Возвращаем среднее время (total / iterations)
  awk "BEGIN{printf \"%.6f\", $total_ms / $iterations}"
}

# Суммирование двух чисел (целые)
int_add() {
  local num1="${1:-0}" num2="${2:-0}"
  echo $((num1 + num2))
}

# Отчистка page cache (если нужно). Требует sudo.
# ВЫЗЫВАЙТЕ ВНИМАТЕЛЬНО: sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
drop_caches() {
  if [ "$(id -u)" -ne 0 ]; then
    echo "drop_caches: needs root; skipping (you may run: sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches')"
    return 1
  else
    sync
    echo 3 > /proc/sys/vm/drop_caches
  fi
}

# Создать файл с размером size_bytes и вывести реальное время создания (ms)
create_file_of_size_ms() {
  local path=$1 size_bytes=$2
  measure_ms dd if=/dev/zero of="$path" bs=1 count=0 seek="$((size_bytes))" 2>/dev/null || true
}

# Удобный echo на stderr
log() {
  echo "$@" >&2
}
