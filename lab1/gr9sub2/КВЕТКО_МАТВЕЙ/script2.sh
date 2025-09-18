#!/bin/bash

LOG_FILE="/var/log/auth.log"

if [ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ]; then
    echo "Файл $LOG_FILE не существует или пуст. Анализ невозможен."
    exit 1
fi

grep -E 'Failed|Invalid' "$LOG_FILE" | 
    grep -oE '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' |  
    sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g' |                      
    sort | 
    uniq -c | 
    sort -nr | 
    head -10                                                     

echo "TOP-10 IP-адресов с неудачными попытками входа (последний октет замаскирован):"