#Лабораторная работа 1: Анализ системных логов

##Цель работы

Освоить инструменты командной строки Linux для анализа текстовых данных, в частности системных логов, используя конвейеры, фильтрацию и агрегацию данных.

##Задания и решения
###А) Частотный анализ слов в syslog (TOP-5)

Цель: Определить 5 самых частых слов в системном логе.

Команда:
```bash
$ cat logs/syslog.log | tr -cs '[:alnum:]' '\n' | tr '[:upper:]' '[:lower:]' | sort | uniq -c | sort -nr | head -n 5
```
Результат:
```bash
   1235 23
   1087 a
   1040 nov
    924 15
    555 24
```
Пояснение команд:
cat — читает файл.
tr -cs '[:alnum:]' '\n' - заменяет все небуквенно-цифровые символы на перевод строки (-c инвертирует, -s объединяет повторения),  разбивая текст на слова по одному на строку.
tr '[:upper:]' '[:lower:]' - приводит все символы к нижнему регистру.
sort - сортирует слова в алфавитном порядке.
uniq -c - подсчитывает количество уникальных вхождений.
sort -nr - сортирует по числу вхождений в порядке от большего к меньшему.
head -n 5 - первые 5 строк.

Ключевые выводы:
Наиболее частые "слова" - это числа (23, 15, 24), что соответствует датам и времени в логах, буква "a" как имя пользователя системы, "nov" соответствует текущему месяцу в временных метках.
    

    
###Б) Неудачные попытки входа (auth.log)
Цель: Найти и проанализировать неудачные попытки входа в систему.

Cтрок для задания у меня не было в логах, пришлось воспользоваться ИИ, чтобы сгенерировать данные для этого. Поэтому используем logs/auth.log.b - иначе все команды ничего бы не вывели.
    
Найдите строки с Failed или Invalid в logs/auth.log.

Команда:
```bash
$ grep -i 'failed\|invalid' logs/auth.log.b
```
Результат:
```bash
Mar 20 10:15:22 ubuntu sshd[1234]: Failed password for root from 192.168.1.15 port 22 ssh2
Mar 20 10:15:25 ubuntu sshd[1234]: Failed password for root from 192.168.1.15 port 22 ssh2
Mar 20 10:16:10 ubuntu sshd[1235]: Invalid user hacker from 203.0.113.45 port 22 ssh2
Mar 20 10:17:30 ubuntu sshd[1236]: Failed password for admin from 10.0.0.25 port 22 ssh2
Mar 20 11:20:15 ubuntu sshd[1345]: Failed password for root from 192.168.1.15 port 22 ssh2
Mar 20 11:21:03 ubuntu sshd[1346]: Invalid user test from 198.51.100.10 port 22 ssh2
Mar 20 12:30:45 ubuntu sshd[1456]: Failed password for john from 192.168.1.15 port 22 ssh2
Mar 20 13:45:12 ubuntu sshd[1567]: Failed password for root from 10.0.0.25 port 22 ssh2
Mar 20 14:20:33 ubuntu sshd[1678]: Invalid user administrator from 203.0.113.45 port 22 ssh2
Mar 20 15:10:22 ubuntu sshd[1789]: Failed password for mysql from 192.168.1.20 port 22 ssh2
```


Извлеките IP‑адреса, посчитайте TOP‑10 источников.

Команды (разобранные на части для логических переходов):
```bash
$ grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b'
```
Результат:
```bash
192.168.1.15
192.168.1.15
203.0.113.45
10.0.0.25
192.168.1.15
198.51.100.10
192.168.1.15
10.0.0.25
203.0.113.45
192.168.1.20
```
Сортируем и выводим топ-10
```bash
$ grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sort | uniq -c | sort -nr | head -n 10
```
Результат:
```bash
4 192.168.1.15
2 203.0.113.45
2 10.0.0.25
1 198.51.100.10
1 192.168.1.20
```
Замаскируйте последний октет IP (например, 192.168.0.15 → 192.168.0.x) с помощью sed.
```bash
$ grep -i 'failed\|invalid' logs/auth.log.b | grep -oE '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' | sort | uniq -c | sort -nr | head -n 10
```
Результат:
```bash
5 192.168.1.x
2 203.0.113.x
2 10.0.0.x
1 198.51.100.x
``` 
    
        
Пояснение:

grep -i — игнорирует регистр.
grep -oE — показывает только совпавшую часть строки (-o) и использует расширенные регулярные выражения (-E).
sed -E 's/(...).../.../g' — команда замены. (...) — группа захвата. \1 — вставка первой группы захвата. Мы захватываем первые три октета и заменяем весь IP на "три октета + .x"

Выводы: IP-адрес 192.168.1.15 является основным источником атак (4 попытки).


###В) Установки пакетов (dpkg.log)

Цель: Определить наиболее часто устанавливаемые пакеты.

По /var/log/dpkg.log посчитайте, какие пакеты устанавливались чаще всего (события install).
Выведите TOP‑10 пакетов с количеством установок.

Выведем строки, связанные с установкой
Команда:
```bash
$ grep ' install ' logs/dpkg.log
```
Результат:
```bash
2025-11-23 15:27:25 install wmdocker:amd64 <none> 1.5-2
2025-11-23 15:27:25 install docker:all <none> 1.5-2
2025-11-23 15:27:50 install libprotobuf-lite23:amd64 <none> 3.12.4-1ubuntu7.22.04.4
2025-11-23 15:27:50 install libprotoc23:amd64 <none> 3.12.4-1ubuntu7.22.04.4
2025-11-23 15:27:50 install libprotobuf-dev:amd64 <none> 3.12.4-1ubuntu7.22.04.4
2025-11-23 15:27:50 install protobuf-compiler:amd64 <none> 3.12.4-1ubuntu7.22.04.4
2025-11-23 15:27:50 install protoc-gen-go:amd64 <none> 1.27.1-1ubuntu0.1
```

Извлекаем названия пакетов, используем awk, чтобы взять имя пакета (0 - вся строка, 1 - 1-ое поле и т.д.)
Команда:
```bash
$ grep ' install ' logs/dpkg.log | awk '{print $4}'
```
Результат:
```bash
wmdocker:amd64
docker:all
libprotobuf-lite23:amd64
libprotoc23:amd64
libprotobuf-dev:amd64
protobuf-compiler:amd64
protoc-gen-go:amd64
```

Убираем все, что после ":"
Команда:
```bash
$ grep ' install ' logs/dpkg.log | awk '{print $4}' | sed 's/:.*//'
```
Результат:
```bash
wmdocker
docker
libprotobuf-lite23
libprotoc23
libprotobuf-dev
protobuf-compiler
protoc-gen-go
```

Сортируем, подсчитываем и выводим TOP-10
Команда:
```bash
$ grep ' install ' logs/dpkg.log | awk '{print $4}' | sed 's/:.*//' | sort | uniq -c | sort -nr | head -n 10
```
Результат:
```bash
1 wmdocker
1 protoc-gen-go
1 protobuf-compiler
1 libprotoc23
1 libprotobuf-lite23
1 libprotobuf-dev
1 docker
```
Выводы: Все пакеты устанавливались по одному разу.

Добавлен скрипт run.sh для воспроизводимости результатов.
ИИ использовался для генерации логов auth.log.b
