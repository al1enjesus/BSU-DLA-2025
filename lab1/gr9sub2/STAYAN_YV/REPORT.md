## Цель: Освоить базовые инструменты командной строки для анализа системных логов.
# Задание А
Код из main не менялся:)
```bash
cat logs/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```
**Результат:** TOP-5 самых частых слов в syslog:
- 988 desktop
- 965 1r6jdge
- 964 sep
- 852 16
- 841 23
# Задание B: Анализ аутентификации
```bash
sudo grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null | head -n 5
echo ""
echo "IP-addresses:"
sudo
grep -E -i "failed|invalid" /var/log/auth.log 2>/dev/null |
grep -E -o "([0-9]{1,3}\.){3}[0-9]{1,3}" |
head -n 5
```
**Результат:** Неудачные попытки входа

Sep 17 01:37:36 DESKTOP-1R6JDGE sudo:   kaiser : TTY=pts/0 ; PWD=/home/kaiser/BSU-DLA-2025/lab1/gr9sub2/STAYAN_YV ; 
USER=root ; COMMAND=/usr/bin/grep -E -i failed|invalid /var/log/auth.log

Неудачных попыток входа (failed password, invalid user) не обнаружено.
# Задание B: Установки пакетов 
```bash
grep -i ' install ' logs/dpkg.log \
| awk {'print $4'} \
| sed -E 's/:.*//' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10
```
**Результат:** Файл dpkg.log пуст
- История установок недоступна через dpkg.log
- Всего в системе: 1454 установленных пакета
- Это нормально для систем с ротацией логов или недавней установкой
# Использование ИИ:
Использовался для объяснения моментов,связанных с поиском файлов log,пояснением пустоты последнего и разъяснением кода.
