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
```
grep -Ei 'Failed|Invalid' logs/auth.log \
| grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' \
| sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g' \
| sort \
| uniq -c \
| sort -nr \
| head -n 10
```



