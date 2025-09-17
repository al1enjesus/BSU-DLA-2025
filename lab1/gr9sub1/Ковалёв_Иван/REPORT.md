# Отчёт по лабораторной работе №1

## Цель работы
Освоить базовые инструменты командной строки для анализа системных логов, логов пакетов, логов аутентификации.

### А) Частотный анализ слов в syslog (TOP‑5)

Команда осталась без изменений:

```bash
# Анализ частоты слов с нормализацией регистра
cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```

Вывод:

```
 112575 00
  97197 09
  96904 5
  95650 vanya
  95647 2025
```

### Б) Неудачные попытки входа (auth.log)
- Найти строки с `Failed` или `Invalid` в `/var/log/auth.log`.

Команда:

```bash
    grep -E '(Failed|Invalid)' /var/log/auth.log
```

Вывод:
```
2025-09-16T20:08:39.028450+03:00 vanya-Legion-5-15ACH6 dbus-daemon[2265]: [session uid=1000 pid=2265] Failed to activate service 'org.freedesktop.portal.Desktop': timed out (service_start_timeout=120000ms)
2025-09-16T20:08:39.055342+03:00 vanya-Legion-5-15ACH6 dbus-daemon[2265]: [session uid=1000 pid=2265] Failed to activate service 'org.freedesktop.impl.portal.desktop.gnome': timed out (service_start_timeout=120000ms)
2025-09-16T18:13:52.943434+03:00 vanya-Legion-5-15ACH6 dbus-daemon[2265]: [session uid=1000 pid=2265] Failed to activate service 'org.freedesktop.portal.Desktop': timed out (service_start_timeout=120000ms)
2025-09-16T18:13:52.978003+03:00 vanya-Legion-5-15ACH6 dbus-daemon[2265]: [session uid=1000 pid=2265] Failed to activate service 'org.freedesktop.impl.portal.desktop.gnome': timed out (service_start_timeout=120000ms)
```

- Извлечь IP‑адреса, посчитайте TOP‑10 источников.

Команда:

```bash
grep -E '(Failed|Invalid)' /var/log/auth.log \
  | grep -E -o '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```

Вывод:
```

```
В auth.log нет логов с информацией по ip, так как не производились попытки установления удалённого доступа.

- Замаскируйте последний октет IP (например, `192.168.0.15 → 192.168.0.x`) с помощью `sed`.

Команда:

```bash
grep -E '(Failed|Invalid)' /var/log/auth.log \
  | grep -E -o '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' \
  | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```

Вывод:

```

```
Та же причина, как и в предыдущем пункте.

### В) Установки пакетов (dpkg.log)
- По `/var/log/dpkg.log` посчитать, какие пакеты устанавливались чаще всего (события `install`).
- Вывести TOP‑10 пакетов с количеством установок.

Команда:

```bash
grep 'install ' /var/log/dpkg.log \
  | awk '{print $4}' \
  | cut -d: -f1 \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```

Вывод:

```
      2 language-pack-ru-base
      2 language-pack-ru
      2 language-pack-gnome-ru-base
      2 language-pack-gnome-ru
      2 gnome-user-docs-ru
      1 zstd
      1 zlib1g
      1 zip
      1 zenity-common
      1 zenity
```

## Использование ИИ
ИИ использовался для написания корректных команд.
