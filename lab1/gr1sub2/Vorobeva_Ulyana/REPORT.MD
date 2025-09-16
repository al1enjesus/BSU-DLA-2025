
# Лабораторная 1. Анализ логов в Linux

## Цель
Изучить работу с логами ОС Linux и базовые утилиты обработки текстовых данных. 
Выполнить частотный анализ слов в системном логе, определить неудачные попытки входа и проанализировать установки пакетов.

---

## Задание A — Частотный анализ syslog

**Команда:**
```bash
cat /var/log/syslog   | tr -cs '[:alnum:]' '\n'   | tr '[:upper:]' '[:lower:]'   | sort   | uniq -c   | sort -nr   | head -n 5
```

**Результат:**
```
 14168 3
 13479 ideapad
 13475 ullecses
 13424 gaming
 13424 15ihu6
```

**Ключевой вывод:** 
Самые частые слова в syslog связаны с именем компьютера (`ideapad-gaming-3-15IHU6`) и пользователем (`ullecses`). Логи содержат системные и пользовательские события.

---

## Задание B — Неудачные попытки входа (auth.log)

**Цель:** найти строки с `Failed` или `Invalid`, извлечь IP, посчитать TOP‑10, замаскировать последний октет.

**Команды:**
```bash
# Собираем строки с Failed/Invalid
grep -E "Failed|Invalid" /var/log/auth.log 2>/dev/null > logs/auth_failed_raw.txt

# Извлекаем IPv4 (если они есть)
grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' logs/auth_failed_raw.txt > logs/auth_failed_ips_raw.txt

# Маскируем последний октет и считаем TOP-10
sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' logs/auth_failed_ips_raw.txt   | sort | uniq -c | sort -nr | head -n 10 > logs/auth_failed_top10_masked.txt
```

**Результат:**  
Файлы пустые, так как в системе не зафиксировано неудачных попыток входа.

**Ключевой вывод:**  
На момент выполнения лабораторной не было неудачных входов в систему, поэтому топ-10 источников атак отсутствует.  

---

## Задание C — Анализ установок пакетов

**Команды:**
```bash
grep "install" /var/log/dpkg.log > dpkg_install.txt
awk '{print $4}' dpkg_install.txt > packages.txt
sort packages.txt | uniq -c | sort -nr | head -n 10 > packages_top10.txt
```

**Результат (`packages_top10.txt`):**
```
      8 installed
      5 half-installed
      1 rospack-tools:amd64
      1 librospack0d:amd64
      1 libonig5:amd64
      1 libjq1:amd64
      1 jq:amd64
```

**Ключевой вывод:** 
Чаще всего в логах фиксировались статусы `installed` и `half-installed`. Среди пакетов встречались: `rospack-tools`, `librospack0d`, `libonig5`, `libjq1`, `jq`.

---

## Как проверяли
- Использовали `head`, `cat`, `wc -l` для проверки файлов и вывода команд. 
- Убедилась, что команды безопасны и не изменяют систему. 
- Проверили наличие файлов логов и корректность пути.

---

## Как использовали AI
- Подсказки по синтаксису `grep`, `awk`, `sed` 
# Лабораторная 1. Анализ логов в Linux

## Цель
Изучить работу с логами ОС Linux и базовые утилиты обработки текстовых данных. 
Выполнить частотный анализ слов в системном логе, определить неудачные попытки входа и проанализировать установки пакетов.

---

## Задание A — Частотный анализ syslog

**Команда:**
```bash
cat /var/log/syslog   | tr -cs '[:alnum:]' '\n'   | tr '[:upper:]' '[:lower:]'   | sort   | uniq -c   | sort -nr   | head -n 5
```

**Результат:**
```
 14168 3
 13479 ideapad
 13475 ullecses
 13424 gaming
 13424 15ihu6
```

**Ключевой вывод:** 
Самые частые слова в syslog связаны с именем компьютера (`ideapad-gaming-3-15IHU6`) и пользователем (`ullecses`). Логи содержат системные и пользовательские события.

---

## Задание B — Неудачные попытки входа (auth.log)

**Цель:** найти строки с `Failed` или `Invalid`, извлечь IP, посчитать TOP‑10, замаскировать последний октет.

**Команды:**
```bash
# Собираем строки с Failed/Invalid
grep -E "Failed|Invalid" /var/log/auth.log 2>/dev/null > logs/auth_failed_raw.txt

# Извлекаем IPv4 (если они есть)
grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' logs/auth_failed_raw.txt > logs/auth_failed_ips_raw.txt

# Маскируем последний октет и считаем TOP-10
sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' logs/auth_failed_ips_raw.txt   | sort | uniq -c | sort -nr | head -n 10 > logs/auth_failed_top10_masked.txt
```

**Результат:**  
Файлы пустые, так как в системе не зафиксировано неудачных попыток входа.

**Ключевой вывод:**  
На момент выполнения лабораторной не было неудачных входов в систему, поэтому топ-10 источников атак отсутствует.  

---

## Задание C — Анализ установок пакетов

**Команды:**
```bash
grep "install" /var/log/dpkg.log > dpkg_install.txt
awk '{print $4}' dpkg_install.txt > packages.txt
sort packages.txt | uniq -c | sort -nr | head -n 10 > packages_top10.txt
```

**Результат (`packages_top10.txt`):**
```
      8 installed
      5 half-installed
      1 rospack-tools:amd64
      1 librospack0d:amd64
      1 libonig5:amd64
      1 libjq1:amd64
      1 jq:amd64
```

**Ключевой вывод:** 
Чаще всего в логах фиксировались статусы `installed` и `half-installed`. Среди пакетов встречались: `rospack-tools`, `librospack0d`, `libonig5`, `libjq1`, `jq`.

---

## Как проверяли
- Использовали `head`, `cat`, `wc -l` для проверки файлов и вывода команд. 
- Убедились, что команды безопасны и не изменяют систему. 
- Проверили наличие файлов логов и корректность пути.

---

## Как использовали AI
- Составление конвейеров команд для анализа логов. 
- Подсказки по синтаксису `grep`, `awk`, `sed` и обработке вывод
# Лабораторная 1. Анализ логов в Linux

## Цель
Изучить работу с логами ОС Linux и базовые утилиты обработки текстовых данных. 
Выполнить частотный анализ слов в системном логе, определить неудачные попытки входа и проанализировать установки пакетов.

---

## Задание A — Частотный анализ syslog

**Команда:**
```bash
cat /var/log/syslog   | tr -cs '[:alnum:]' '\n'   | tr '[:upper:]' '[:lower:]'   | sort   | uniq -c   | sort -nr   | head -n 5
```

**Результат:**
```
 14168 3
 13479 ideapad
 13475 ullecses
 13424 gaming
 13424 15ihu6
```

**Ключевой вывод:** 
Самые частые слова в syslog связаны с именем компьютера (`ideapad-gaming-3-15IHU6`) и пользователем (`ullecses`). Логи содержат системные и пользовательские события.

---

## Задание B — Неудачные попытки входа (auth.log)

**Цель:** найти строки с `Failed` или `Invalid`, извлечь IP, посчитать TOP‑10, замаскировать последний октет.

**Команды:**
```bash
# Собираем строки с Failed/Invalid
grep -E "Failed|Invalid" /var/log/auth.log 2>/dev/null > logs/auth_failed_raw.txt

# Извлекаем IPv4 (если они есть)
grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' logs/auth_failed_raw.txt > logs/auth_failed_ips_raw.txt

# Маскируем последний октет и считаем TOP-10
sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' logs/auth_failed_ips_raw.txt   | sort | uniq -c | sort -nr | head -n 10 > logs/auth_failed_top10_masked.txt
```

**Результат:**  
Файлы пустые, так как в системе не зафиксировано неудачных попыток входа.

**Ключевой вывод:**  
На момент выполнения лабораторной не было неудачных входов в систему, поэтому топ-10 источников атак отсутствует.  

---

## Задание C — Анализ установок пакетов

**Команды:**
```bash
grep "install" /var/log/dpkg.log > dpkg_install.txt
awk '{print $4}' dpkg_install.txt > packages.txt
sort packages.txt | uniq -c | sort -nr | head -n 10 > packages_top10.txt
```

**Результат (`packages_top10.txt`):**
```
      8 installed
      5 half-installed
      1 rospack-tools:amd64
      1 librospack0d:amd64
      1 libonig5:amd64
      1 libjq1:amd64
      1 jq:amd64
```

**Ключевой вывод:** 
Чаще всего в логах фиксировались статусы `installed` и `half-installed`. Среди пакетов встречались: `rospack-tools`, `librospack0d`, `libonig5`, `libjq1`, `jq`.

---

## Как проверяли
- Использовали `head`, `cat`, `wc -l` для проверки файлов и вывода команд. 
- Убедились, что команды безопасны и не изменяют систему. 
- Проверили наличие файлов логов и корректность пути.

---

## Как использовали AI
- Составление конвейеров команд для анализа логов. 
- Подсказки по синтаксису `grep`, `awk`, `sed` и обработке вывода
# Лабораторная 1. Анализ логов в Linux

## Цель
Изучить работу с логами ОС Linux и базовые утилиты обработки текстовых данных. 
Выполнить частотный анализ слов в системном логе, определить неудачные попытки входа и проанализировать установки пакетов.

---

## Задание A — Частотный анализ syslog

**Команда:**
```bash
cat /var/log/syslog   | tr -cs '[:alnum:]' '\n'   | tr '[:upper:]' '[:lower:]'   | sort   | uniq -c   | sort -nr   | head -n 5
```

**Результат:**
```
 14168 3
 13479 ideapad
 13475 ullecses
 13424 gaming
 13424 15ihu6
```

**Ключевой вывод:** 
Самые частые слова в syslog связаны с именем компьютера (`ideapad-gaming-3-15IHU6`) и пользователем (`ullecses`). Логи содержат системные и пользовательские события.

---

## Задание B — Неудачные попытки входа (auth.log)

**Цель:** найти строки с `Failed` или `Invalid`, извлечь IP, посчитать TOP‑10, замаскировать последний октет.

**Команды:**
```bash
# Собираем строки с Failed/Invalid
grep -E "Failed|Invalid" /var/log/auth.log 2>/dev/null > logs/auth_failed_raw.txt

# Извлекаем IPv4 (если они есть)
grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' logs/auth_failed_raw.txt > logs/auth_failed_ips_raw.txt

# Маскируем последний октет и считаем TOP-10
sed -E 's/([0-9]+\.[0-9]+\.[0-9]+\.)[0-9]+/\1x/g' logs/auth_failed_ips_raw.txt   | sort | uniq -c | sort -nr | head -n 10 > logs/auth_failed_top10_masked.txt
```

**Результат:**  
Файлы пустые, так как в системе не зафиксировано неудачных попыток входа.

**Ключевой вывод:**  
На момент выполнения лабораторной не было неудачных входов в систему, поэтому топ-10 источников атак отсутствует.  

---

## Задание C — Анализ установок пакетов

**Команды:**
```bash
grep "install" /var/log/dpkg.log > dpkg_install.txt
awk '{print $4}' dpkg_install.txt > packages.txt
sort packages.txt | uniq -c | sort -nr | head -n 10 > packages_top10.txt
```

**Результат (`packages_top10.txt`):**
```
      8 installed
      5 half-installed
      1 rospack-tools:amd64
      1 librospack0d:amd64
      1 libonig5:amd64
      1 libjq1:amd64
      1 jq:amd64
```

**Ключевой вывод:** 
Чаще всего в логах фиксировались статусы `installed` и `half-installed`. Среди пакетов встречались: `rospack-tools`, `librospack0d`, `libonig5`, `libjq1`, `jq`.

---

## Как проверяли
- Использовали `head`, `cat`, `wc -l` для проверки файлов и вывода команд. 
- Убедились, что команды безопасны и не изменяют систему. 
- Проверили наличие файлов логов и корректность пути.

---

## Как использовали AI
- Подсказки по синтаксису `grep`, `awk`, `sed`. 

---

## Итог
Все задания выполнены: 
- **A** — частотный анализ слов syslog выполнен и результаты сохранены; 
- **B** — неудачные попытки входа проверены, в системе их не было; 
- **C** — подсчёт установок пакетов выполнен и TOP‑10 получен. . 
---
