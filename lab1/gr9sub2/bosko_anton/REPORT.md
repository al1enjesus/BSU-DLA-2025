## Задание А

Конвейер остался почти без изменений
```bash
cat logs/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```
Вывод: 
```
  8382 00
  4134 virtual
  4086 qemu
  4084 09
  4071 machine
```

## Задание Б

- Найдите строки с Failed или Invalid в /var/log/auth.log.
- Извлеките IP‑адреса, посчитайте TOP‑10 источников.
- Замаскируйте последний октет IP (например, 192.168.0.15 → 192.168.0.x) с помощью sed.
```bash
grep -Ei 'Failed|Invalid' logs/auth.log \
| grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' \
| sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10
```
Пояснение - в логах нету информации по Айпи. Есть только одна строчка со статусом Failed: `2025-09-12T13:19:24.407766+00:00 anton-QEMU-Virtual-Machine dbus-daemon[953]: [system] Failed to activate service 'org.bluez': timed out (service_start_timeout=25000ms)`

## Задание В

- По /var/log/dpkg.log посчитайте, какие пакеты устанавливались чаще всего (события install).
- Выведите TOP‑10 пакетов с количеством установок.
```bash
grep -i ' install ' logs/dpkg.log \
| awk {'print $4'} \
| sed -E 's/:.*//' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10

      1 zstd
      1 zlib1g
      1 zip
      1 zenity-common
      1 zenity
      1 yelp-xsl
      1 yelp
      1 yaru-theme-sound
      1 yaru-theme-icon
      1 yaru-theme-gtk
```

# Использование ИИ
Использовал с целью изучения команд, просил помочь написать регулярные выражения, а после увидел нужное РВ в подсказках :)

