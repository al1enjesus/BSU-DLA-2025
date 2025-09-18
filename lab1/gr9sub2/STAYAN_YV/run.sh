#!/bin/bash
echo "A"
cat logs/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
echo "B1"
sudo grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null | head -n 5
echo ""
echo "IP-addresses:"
sudo grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null | grep -E -o "([0-9]{1,3}\.){3}[0-9]{1,3}" | head -n 5
echo "B2"
grep -i ' install ' logs/dpkg.log \
| awk {'print $4'} \
| sed -E 's/:.*//' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10
