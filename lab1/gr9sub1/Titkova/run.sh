#!/bin/bash

# Создаем директорию для результатов
mkdir -p results

echo "А) Частотный анализ слов в syslog (TOP-5)"


echo "Команда:"
echo "cat logs/syslog.log | tr -cs '[:alnum:]' '\n' | tr '[:upper:]' '[:lower:]' | sort | uniq -c | sort -nr | head -n 5"
echo
echo "Результат:"
cat logs/syslog.log | tr -cs '[:alnum:]' '\n' | tr '[:upper:]' '[:lower:]' | sort | uniq -c | sort -nr | head -n 5 | tee results/top5_words.txt
echo


echo "Б) Неудачные попытки входа (auth.log)"

echo "1. Поиск строк с Failed или Invalid:"
echo "Команда: grep -i 'failed\|invalid' logs/auth.log.b"
echo
grep -i 'failed\|invalid' logs/auth.log.b | tee results/failed_attempts.txt
echo

echo "2. Извлечение IP-адресов:"
echo "Команда: grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b'"
echo
grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | tee results/ip_addresses.txt
echo

echo "3. TOP-10 IP-адресов:"
echo "Команда: grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sort | uniq -c | sort -nr | head -n 10"
echo
grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sort | uniq -c | sort -nr | head -n 10 | tee results/top10_ips.txt
echo

echo "4. TOP-10 IP-адресов с маскировкой:"
echo "Команда: grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' | sort | uniq -c | sort -nr | head -n 10"
echo
grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' | sort | uniq -c | sort -nr | head -n 10 | tee results/top10_masked_ips.txt
echo

echo "В) Установки пакетов (dpkg.log)"

echo "1. Строки связанные с установкой:"
echo "Команда: grep ' install ' logs/dpkg.log"
echo
grep ' install ' logs/dpkg.log | tee results/install_lines.txt
echo

echo "2. Извлечение названий пакетов:"
echo "Команда: grep ' install ' logs/dpkg.log | awk '{print \$4}'"
echo
grep ' install ' logs/dpkg.log | awk '{print $4}' | tee results/package_names.txt
echo

echo "3. Очистка имен пакетов:"
echo "Команда: grep ' install ' logs/dpkg.log | awk '{print \$4}' | sed 's/:.*//'"
echo
grep ' install ' logs/dpkg.log | awk '{print $4}' | sed 's/:.*//' | tee results/cleaned_packages.txt
echo

echo "4. TOP-10 устанавливаемых пакетов:"
echo "Команда: grep ' install ' logs/dpkg.log | awk '{print \$4}' | sed 's/:.*//' | sort | uniq -c | sort -nr | head -n 10"
echo
grep ' install ' logs/dpkg.log | awk '{print $4}' | sed 's/:.*//' | sort | uniq -c | sort -nr | head -n 10 | tee results/top10_packages.txt
echo

echo "Все результаты сохранены в директории 'results/'"
echo
echo "Созданные файлы:"
ls -la results/
