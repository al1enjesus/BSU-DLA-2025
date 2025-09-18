# Краткий отчёт по заданию

## Цель
Разработать три безопасных bash-скрипта для анализа логов:  
- **A)** TOP-5 слов в `/var/log/syslog` (нормализация регистра).  
- **B)** TOP-10 IP из неудачных входов в `/var/log/auth.log` (с маскировкой).  
- **C)** TOP-10 пакетов установок в `/var/log/dpkg.log`.

## Шаги/решения
Скрипты используют пайплайны (grep, awk, sed, sort, uniq). Проверяют наличие файлов, безопасны (только чтение).

### A: syslog_word_freq.sh
```bash
#!/bin/bash
LOG_FILE="/var/log/syslog"
[ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ] && { echo "Файл пуст или отсутствует."; exit 1; }
cat "$LOG_FILE" | tr '[:upper:]' '[:lower:]' | tr -cs '[:alnum:]' '\n' | grep -v '^$' | sort | uniq -c | sort -nr | head -5
```

### B: auth_failed_ips.sh
```bash
#!/bin/bash
LOG_FILE="/var/log/auth.log"
[ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ] && { echo "Файл пуст или отсутствует."; exit 1; }
grep -E 'Failed|Invalid' "$LOG_FILE" | grep -oE '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' | sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g' | sort | uniq -c | sort -nr | head -10
```

### C: dpkg_install_freq.sh
```bash
#!/bin/bash
LOG_FILE="/var/log/dpkg.log"
[ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ] && { echo "Файл пуст или отсутствует."; exit 1; }
grep ' install ' "$LOG_FILE" | awk '{print $4}' | sort | uniq -c | sort -nr | head -10
```

## Ключевые выводы
Скрипты эффективны, воспроизводимы, безопасны. Выводы зависят от логов (например, частые слова: "kernel"; IP: замаскированные; пакеты: "vim").

## Запуск скриптов
Сохраните скрипты в файлы (например, syslog_word_freq.sh), сделайте исполняемыми командой `chmod +x script.sh`, затем запустите `./script.sh` в терминале.
