# Лабораторная 1 — Bash: анализ логов

Короткая практика по работе с файлами логов в Linux с использованием базовых инструментов командной строки.

## Среда и данные

WSL2

- Логи для анализа:
    - `/var/log/syslog`
    - `/var/log/auth.log`
    - `/var/log/dpkg.log`

## Задания

### А) Частотный анализ слов в syslog (TOP‑5)
```bash
cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```

Результат:

```
1315 00
1247 09
1233 03
1224 2025
1218 vgp8iq4
```

Вывод: самые частые слова - дата и время


### Б) Неудачные попытки входа (auth.log)

Команда:
```
grep -E 'Failed|Invalid' /var/log/auth.log \
  | grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' \
  | sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```

Результат:
Неудачных попыток входа нет, так как не найдено логов в результате выполнения команды


### В) Установки пакетов (dpkg.log)
   
Команда:
```
grep "install" /var/log/dpkg.log \
  | awk '{print $4}' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 10
```

Результат:

```
1 zlib1g:amd64
1 xz-utils:amd64
1 xxd:amd64
1 xml-core:all
1 xkb-data:all
1 xdg-user-dirs:amd64
1 xauth:amd64
1 x11-common:all
1 wsl-setup:amd64
1 wsl-pro-service:amd64
```

Вывод: в результатах видны установки системных компонентов


AI помогал в составлении и объяснении регулярок и с отчетом