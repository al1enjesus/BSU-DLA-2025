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
  - `8382 00`
  - `4134 virtual`
  - `4086 qemu`
  - `4084 09`
  - `4071 machine`
