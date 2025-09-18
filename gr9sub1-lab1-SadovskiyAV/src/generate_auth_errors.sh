#!/bin/bash
IPS=("192.168.1." "10.0.0." "172.16.0." "203.0.113." "198.51.100.")
USERS=("root" "admin" "test" "user" "ubuntu")

for i in {1..100}; do
    IP="${IPS[$RANDOM % ${#IPS[@]}]}$((RANDOM % 255))"
    USER="${USERS[$RANDOM % ${#USERS[@]}]}"
    echo "$(date '+%b %d %H:%M:%S') server sshd[12345]: Failed password for $USER from $IP port 22 ssh2" | sudo tee -a /var/log/auth.log >/dev/null
    echo "$(date '+%b %d %H:%M:%S') server sshd[12345]: Invalid user $USER from $IP port 22" | sudo tee -a /var/log/auth.log >/dev/null
    sleep 0.1
done
