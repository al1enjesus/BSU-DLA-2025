# Лабораторная 1 - Bash: анализ логов
Цель: изучение основ работы с командами навигации, поиска, фильтрации, агрегации, парсинга. Создание своего конвейера.

Среда: Linux (Debian 12, KDE Plasma)

Анализируемые логи:
- /var/log/syslog
- /var/log/auth.log
- /var/log/dpkg.log

Замечание: почему-то на Debian 12 нормально не поставился rsyslog. Для исправления этой проблемы достаточно выполнить следующие комманды:
```
sudo apt install --reinstall rsyslog
sudo apt install wsl
sudo systemctl status rsyslog
```

Так же возможно понадобится log out из системы или ее полный перезапуск.

# Ход работы
### Задание А
Цель: посчитать 5 самых частых "слов" в /var/log/syslog

Команда:
```
cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```

Объяснение:
- tr -cs '[:alnum:]' '\n' - Заменяет все НЕ-буквенно-цифровые символы на переводы строк и сжимает повторы	"Error: Connection (failed!)" → "Error" + "Connection" + "failed"
- tr '[:upper:]' '[:lower:]' - Приводит все буквы к нижнему регистру	"Error" → "error", "Connection" → "connection"
- sort - Сортирует слова в алфавитном порядке	"error", "connection", "failed" → "connection", "error", "failed"
- uniq -c - Удаляет повторы и показывает количество вхождений	"error", "error", "failed" → "2 error", "1 failed"
- sort -nr - Сортирует численно по убыванию	"1 failed", "2 error" → "2 error", "1 failed"
- head -n 5 - Выводит только первые 5 строк

output:
```
3835 03
3003 00
2641 09
2614 kapusha
2609 debian
```

Интерпретация:
Первая колнка - кол-во повторений
Вторая - само слово
Таким образом, "03" - самое частое слово, 3835 повторений

### Задание Б
Цель: найти неудачные попытки аутентификации в логе /var/log/auth.log и вывести ТОП 10 источников ошибки, замаскировав последний октет IP.
Неудачных попыток в моем логе не оказалось, а плодить ошибки с sudo мне не хотелось. LLM Модель написала неплохую quick-start последовательность комманд, которая заполняет лог несколькими такими энтри.

Обернуть такую штуку в скрипт и запустить можно следующим набором:
```
# Создаем скрипт для генерации логов
cat > generate_auth_errors.sh << 'EOF'
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
EOF

chmod +x generate_auth_errors.sh
sudo ./generate_auth_errors.sh
```

Суть такого генератора - "постучать" по нескольким адресам и нескольким пользователям через ssh. Скрипт можно найти в папочке src/

Команда для создания ТОП 10:
```
grep -E "Failed|Invalid" /var/log/auth.log \
  | grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' \
  | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -10
```

Объяснение:
- grep -E "Failed|Invalid" /var/log/auth.log - ищет строки с "Failed" ИЛИ "Invalid"
- grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' - извлекает IP-адреса
- sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' - маскирует последний октет (заменяет на .x)
- sort | uniq -c - подсчитывает количество вхождений каждого IP
- sort -nr - сортирует по убыванию количества
- head -10 - выводит топ-10 результатов

Output:
```
48 172.16.0.x
44 192.168.1.x
40 10.0.0.x
38 198.51.100.x
30 203.0.113.x
```

Интерпретация:
Первая колонка - кол-во неудачных попыток аутентификации. Вторая - айпишник с замененым последним октетом.

### Задание В
Цель: проанализировать /var/log/dpkg.log и вывести ТОП 10 пакетов по количеству установок.

Команда:
```
grep "install " /var/log/dpkg.log \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -10 \
```

Объяснение:
- awk '{print $4}' - Извлекает название пакета (4-е поле)
- sort - Сортирует названия пакетов
- uniq -c Подсчитывает количество установок каждого пакета
- sort -nr Сортирует по убыванию количества установок
- head -10 Выводит TOP-10 пакетов

Output:
```
1 xsltproc:amd64
1 wsl:all
1 slirp4netns:amd64
1 rsyslog:amd64
1 pigz:amd64
1 libslirp0:amd64
1 liblognorm5:amd64
1 libip6tc2:amd64
1 libfastjson4:amd64
1 libestr0:amd64
```

