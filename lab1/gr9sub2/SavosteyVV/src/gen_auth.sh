#!/bin/bash

# Файл куда пишем
OUTPUT="auth.log"

# Очистим файл если был
> "$OUTPUT"

# Массивы данных для рандомизации
USERS=("root" "admin" "kali" "support" "user" "oracle")
IPS=("192.168.1.55" "10.0.0.13" "45.33.22.11" "192.168.1.55" "192.168.1.55" "172.16.0.5")
# Генерируем 200 записей
for i in {1..200}; do
    USER=${USERS[$RANDOM % ${#USERS[@]}]}
    IP=${IPS[$RANDOM % ${#IPS[@]}]}
    PORT=$((10000 + RANDOM % 50000))
    TIME=$(date "+%b %d %H:%M:%S")

    # 80% Failed password, 20% Invalid user
    if [ $((RANDOM % 5)) -eq 0 ]; then
         echo "$TIME kali sshd[1337]: Failed password for invalid user $USER from $IP port $PORT ssh2" >> "$OUTPUT"
    else
         echo "$TIME kali sshd[1337]: Failed password for $USER from $IP port $PORT ssh2" >> "$OUTPUT"
    fi
done
