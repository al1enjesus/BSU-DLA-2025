# Лабороторная работа №1

*Цель: Освоить базовые инструменты командной строки для анализа системных логов.*

## Задание А:

cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | grep -v '^$' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5 \

*Результат:*

   1811 ruslan
   1806 sep
   1804 18
   1022 systemd
    937 1

## Задание Б:

grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null | head -n 5
echo ""
echo "IP-addresses:"
sudo
grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null |
grep -E -o "([0-9]{1,3}\.){3}[0-9]{1,3}" |
head -n 5

*Результат:*

Sep 18 21:59:35 Ruslan sudo: russlan68 : TTY=pts/0 ; PWD=/home/russlan68/lab1/gr6sub1/Ванли_Руслан ; USER=root ; COMMAND=/usr/bin/grep -Ei failed|invalid /var/log/auth.log
Sep 18 21:59:44 Ruslan sudo: russlan68 : TTY=pts/0 ; PWD=/home/russlan68/lab1/gr6sub1/Ванли_Руслан ; USER=root ; COMMAND=/usr/bin/grep -Ei failed|invalid /var/log/auth.log
Sep 19 00:21:40 Ruslan sudo: russlan68 : TTY=pts/0 ; PWD=/home/russlan68 ; USER=root ; COMMAND=/usr/bin/grep -E -i failed|invalid /var/log/auth.log


## Задание Б:

 grep -i ' install ' /var/log/dpkg.log \
| awk {'print $4'} \
| sed -E 's/:.*//' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10

*Результат:*

файл dpkg - пустой

**Использование ИИ**
Использовал ChatGPT для уточнения задания, подбора и объяснения команд Bash, а также для шаблона отчёта.