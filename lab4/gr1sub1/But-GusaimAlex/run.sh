#!/bin/bash
# Лабораторная работа №4: Анализ системных вызовов и CPU/IO поведения программ (ls, cat, grep)

mkdir -p logs

echo "=== Анализ программы ls ==="
strace -c ls > logs/ls_strace.log 2>&1
echo "--- perf stat ls ---" > logs/ls_perf.log
if sudo -n true 2>/dev/null; then
  sudo perf stat ls >> logs/ls_perf.log 2>&1
else
  echo "⚠️ perf требует root-доступ. Запустите скрипт с sudo для полного анализа." >> logs/ls_perf.log
fi

echo "=== Анализ программы cat ==="
strace -c cat /etc/hostname > logs/cat_strace.log 2>&1
echo "--- perf stat cat ---" > logs/cat_perf.log
if sudo -n true 2>/dev/null; then
  sudo perf stat cat /etc/hostname >> logs/cat_perf.log 2>&1
else
  echo "⚠️ perf требует root-доступ. Запустите скрипт с sudo для полного анализа." >> logs/cat_perf.log
fi

echo "=== Анализ программы grep ==="
echo "test test test" > logs/tmpfile.txt
strace -c grep test logs/tmpfile.txt > logs/grep_strace.log 2>&1
echo "--- perf stat grep ---" > logs/grep_perf.log
if sudo -n true 2>/dev/null; then
  sudo perf stat grep test logs/tmpfile.txt >> logs/grep_perf.log 2>&1
else
  echo "⚠️ perf требует root-доступ. Запустите скрипт с sudo для полного анализа." >> logs/grep_perf.log
fi

echo "✅ Анализ завершен. Результаты сохранены в папке logs/."
