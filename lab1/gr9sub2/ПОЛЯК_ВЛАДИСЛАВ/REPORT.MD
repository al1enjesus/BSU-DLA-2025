# Лабораторная 1 — Bash: анализ логов

## Цель
Познакомиться с базовыми инструментами Bash для анализа системных логов.


## Задание 1 — script1.sh

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

## Вывод 1
  16443 00
   8844 ubuntu
   7958 09
   7918 2025
   7906 18t21
TOP-5 самых частых слов в /var/log/syslog:

## Задание 2 — script2.sh

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

## Вывод 2
TOP-10 IP-адресов с неудачными попытками входа (последний октет замаскирован):

## Задание 3 — script3.sh

grep ' install ' /var/log/dpkg.log \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
  
## Вывод 3
      1 zstd:amd64
      1 zlib1g:amd64
      1 zip:amd64
      1 zfsutils-linux:amd64
      1 zfs-zed:amd64
      1 zenity:amd64
      1 zenity-common:all
      1 yelp:amd64
      1 yelp-xsl:all
      1 yaru-theme-sound:all

## Общий отчет
Скрипты эффективны, безопасны. AI использовался для написания отчёта и ознакомлением с ОС Linux.
