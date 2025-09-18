#!/bin/bash

LOG_FILE="/var/log/dpkg.log"

if [ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ]; then
    echo "Файл $LOG_FILE не существует или пуст. Анализ невозможен."
    exit 1
fi

grep ' install ' "$LOG_FILE" | 
    awk '{print $4}' |  
    sort | 
    uniq -c | 
    sort -nr | 
    head -10             

echo "TOP-10 самых часто устанавливаемых пакетов:"