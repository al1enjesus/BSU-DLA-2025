# Отчёт по лабораторной работе 1

**Студент**: Луцкий Вадим  
**Группа**: gr9sub1  

# Задание А: Частотный анализ слов в syslog (TOP‑5)
- Подсчитайте 5 самых частых “слов” в `/var/log/syslog` (нормализуйте регистр, отделяйте слова).

**Pipeline**
```bash
cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```
**Результат:**
 1. 884026 ux3404va - Имя хоста
 2. 492707 09 - какое-то число, например, дата
 3. 480975 03 - какое-то число, например, дата
 4. 478850 00 - вероятно, часть временной метки
 5. 445626 2025 - текущий год из меток времени
   
**Вариация пайплайна:**
```bash
tail -1000 /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```
**Результат:**
 1. 2000 ux3404va - Имя хоста
 2. 1146 03 - какое-то число, например, дата
 3. 1120 09 - какое-то число, например, дата
 4. 1089 2025 - текущий год из меток времени
 5. 1075 00 - вероятно, часть временной метки

# Задание Б: Анализ неудачных попыток входа (auth.log)
- Найдите строки с `Failed` или `Invalid` в `/var/log/auth.log`.
- Извлеките IP‑адреса, посчитайте TOP‑10 источников. 
- Замаскируйте последний октет IP (например, `192.168.0.15 → 192.168.0.x`) с помощью `sed`.
  - Подсказка: используйте `grep -E`, `awk`/`cut`, `sort | uniq -c | sort -nr`, и `sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g'`.

**Pipeline**
```bash
grep -E 'Failed|Invalid' /var/log/auth.log \
  | grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' \
  | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```
**Результат:**
- Пустой вывод. Выполнив 
```bash
sudo grep -E 'Failed|Invalid' /var/log/auth.log
```
я проверил, что в систему еще не было неудачных попыток входа, пустой вывод не из-за
некорректного пайплайна 


# Задание В: Анализ установленных пакетов (dpkg.log)
- По `/var/log/dpkg.log` посчитайте, какие пакеты устанавливались чаще всего (события `install`).
- Выведите TOP‑10 пакетов с количеством установок.

**Pipeline**
```bash
 grep 'install ' /var/log/dpkg.log \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```
Результат:
1. 1 linux-tools-6.14.0-29-generic:amd64
2. 1 linux-modules-extra-6.14.0-29-generic:amd64
3. 1 linux-modules-6.14.0-29-generic:amd64
4. 1 linux-image-6.14.0-29-generic:amd64
5. 1 linux-hwe-6.14-tools-6.14.0-29:amd64
6. 1 linux-hwe-6.14-headers-6.14.0-29:all
7. 1 linux-headers-6.14.0-29-generic:amd64

Мы видим только 7 пакетов, потому что текущий лог файл недавно был обновлен
Если просмотреть предыдущий(архивный) лог файл, то увидим иной результат

**Pipeline**
```bash
grep 'install ' /var/log/dpkg.log.1 \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```
**Результат**
1. 1 netbird-ui:amd64
2. 1 netbird:amd64
3. 1 linux-tools-6.14.0-28-generic:amd64
4. 1 linux-modules-extra-6.14.0-28-generic:amd64
5. 1 linux-modules-6.14.0-28-generic:amd64
6. 1 linux-image-6.14.0-28-generic:amd64
7. 1 linux-hwe-6.14-tools-6.14.0-28:amd64
8. 1 linux-hwe-6.14-headers-6.14.0-28:all
9. 1 linux-headers-6.14.0-28-generic:amd64

Если просмотреть за всю историю логов, то получим
**Pipeline**
```bash
zgrep 'install ' /var/log/dpkg.log* \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```
**Результат**
1.  4 discord:amd64
2.  2 libllvm17t64:amd64
3.  2 google-chrome-stable:amd64
4.  1 zstd:amd64
5.  1 zsh-common:all
6.  1 zsh:amd64
7.  1 zlib1g-dev:amd64
8.  1 zlib1g:amd64
9.  1 zip:amd64
10. 1 zenity-common:all
 
 
 
 
 
 
 
 