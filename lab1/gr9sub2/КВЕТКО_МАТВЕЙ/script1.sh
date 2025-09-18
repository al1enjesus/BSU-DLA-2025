#!/bin/bash

LOG_FILE="/var/log/syslog"

if [ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ]; then
    echo "Файл $LOG_FILE не существует или пуст. Анализ невозможен."
    exit 1
fi

cat "$LOG_FILE" | 
    tr '[:upper:]' '[:lower:]' |  
    tr -cs '[:alnum:]' '\n' |     
    grep -v '^$' |                
    sort | 
    uniq -c | 
    sort -nr | 
    head -5                       

echo "TOP-5 самых частых слов в $LOG_FILE:"