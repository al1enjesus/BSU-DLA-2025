# Задание А)

## Цели
Подсчитать 5 самых частых “слов” в /var/log/syslog 

## Шаги/решения
```bash
cat logs/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```
   
## Выводы
```   
      7268 00
      5121 09
      5105 virtualbox
      5097 dasha
      5086 2025
```

## Использование AI
Для объяснения работы некоторых команд

# Задание Б)

## Цели
Найти строки с Failed или Invalid в /var/log/auth.log.Извлечь IP‑адреса, посчитать TOP‑10 источников. Замаскировать последний октет IP (например, 192.168.0.15 → 192.168.0.x) с помощью sed.

## Шаги/решения
```
grep -E 'Failed|Invalid' logs/auth.log
```
Результат: 
```
2025-09-12T16:15:10.848303+03:00 dasha-VirtualBox dbus-daemon[833]: [system] Failed to activate service 'org.bluez': timed out (service_start_timeout=25000ms)
2025-09-14T14:18:11.255650+03:00 dasha-VirtualBox dbus-daemon[846]: [system] Failed to activate service 'org.bluez': timed out (service_start_timeout=25000ms)
```
Вывода не было, логов с айпишниками нет:
```
grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' auth.log\                                                     
  | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' \
  | sort | uniq -c | sort -nr | head -n 10
```
   
## Выводы
Получены строки с Failed
   
## Использование AI
Для объяснения работы некоторых команд

# Задание В)

## Цели
Установить пакеты (dpkg.log)

## Шаги/решения

```
grep ' install ' dpkg.log \
  | awk '{print $4}' \
  | sort | uniq -c | sort -nr | head -n 10
```
   
## Выводы
```  
      1 zstd:amd64
      1 zlib1g:amd64
      1 zip:amd64
      1 zenity-common:all
      1 zenity:amd64
      1 yelp-xsl:all
      1 yelp:amd64
      1 yaru-theme-sound:all
      1 yaru-theme-icon:all
      1 yaru-theme-gtk:all
```





