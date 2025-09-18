# Лабораторная 1 — Bash: анализ логов
Кушмар Элина 9 группа


## Цель
Изучить базовые инструменты командной строки. С их помощью провести анализ логов в Linux.

## Задания

### А) Частотный анализ слов в syslog (TOP‑5)
- Подсчитайте 5 самых частых “слов” в `/var/log/syslog` (нормализуйте регистр, отделяйте слова).
- Пример конвейера (можно адаптировать):
```bash
cat /var/log/syslog \
  | tr -cs '[:alnum:]' '\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort \
  | uniq -c \
  | sort -nr \
  | head -n 5
```

#### Результат выполнения команды
```
  5626 00
  2852 ubuntu
  2715 2025
  2705 09
  2494 18t16
```
#### Использование AI
Использовала AI, чтобы разобраться в том, что такое конвейер.

#### Вывод
Конвейер - это способ соединить несколько команд так, 
чтобы результат одной команды автоматически передавался на вход другой команды.

Команда sort отвечает за сортировку в указанном порядке, head - за вывод первых n строк.



### Б) Неудачные попытки входа (auth.log)
- Найдите строки с `Failed` или `Invalid` в `/var/log/auth.log`.
  
#### Команда (для auth.log)
  ```bash
  grep -E 'Failed|Invalid' /var/log/auth.log
  ```
#### Результат выполнения команды
```
2025-09-18T17:29:06.446020+00:00 ubuntu dbus-daemon[1368]: [system] Failed to activate service 'org.bluez': timed out (service_start_timeout=25000ms)
```

#### Команда (для syslog, так как в auth.log всего одна строка с Failed или Invalid)
```bash
grep -E 'Failed|Invalid' /var/log/syslog
```
#### Результат выполнения команды (10 из 86 строк)
```
2025-09-18T16:43:00.137095+00:00 ubuntu systemd[1]: pd-mapper.service: Failed with result 'exit-code'.
2025-09-18T16:43:00.137120+00:00 ubuntu systemd[1]: Failed to start pd-mapper.service - Qualcomm PD mapper service.
2025-09-18T16:43:13.127028+00:00 ubuntu wireplumber[1894]: default: Failed to get percentage from UPower: org.freedesktop.DBus.Error.NameHasNoOwner
2025-09-18T16:43:13.407835+00:00 ubuntu gnome-session[2030]: gnome-session-binary[2030]: GnomeDesktop-WARNING: Could not create transient scope for PID 2039: GDBus.Error:org.freedesktop.DBus.Error.UnixProcessIdUnknown: Failed to set unit properties: No such process
2025-09-18T16:43:13.407870+00:00 ubuntu gnome-session-binary[2030]: GnomeDesktop-WARNING: Could not create transient scope for PID 2039: GDBus.Error:org.freedesktop.DBus.Error.UnixProcessIdUnknown: Failed to set unit properties: No such process
2025-09-18T16:43:13.408396+00:00 ubuntu gnome-session[2030]: gnome-session-binary[2030]: GnomeDesktop-WARNING: Could not create transient scope for PID 2041: GDBus.Error:org.freedesktop.DBus.Error.UnixProcessIdUnknown: Failed to set unit properties: No such process
2025-09-18T16:43:13.408421+00:00 ubuntu gnome-session-binary[2030]: GnomeDesktop-WARNING: Could not create transient scope for PID 2041: GDBus.Error:org.freedesktop.DBus.Error.UnixProcessIdUnknown: Failed to set unit properties: No such process
2025-09-18T16:57:20.260156+00:00 ubuntu org.gnome.Settings.desktop[6122]: MESA: error: Failed to create virtgpu AddressSpaceStream
2025-09-18T16:57:20.260168+00:00 ubuntu org.gnome.Settings.desktop[6122]: MESA: error: DRM_VIRTGPU_RESOURCE_CREATE_BLOB failed with Invalid argument
2025-09-18T16:57:20.260178+00:00 ubuntu org.gnome.Settings.desktop[6122]: MESA: error: Failed to create virtgpu AddressSpaceStream
```

- Извлеките IP‑адреса, посчитайте TOP‑10 источников.
- Замаскируйте последний октет IP (например, `192.168.0.15 → 192.168.0.x`) с помощью `sed`.
  - Подсказка: используйте `grep -E`, `awk`/`cut`, `sort | uniq -c | sort -nr`, и `sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g'`.
 
#### Использование AI
Спросила AI о том, за что отвечают -E и -Eo при выполнении команды grep.

#### Вывод
Команда grep выполняет поиск в файле с использованием регулярных выражений

### В) Установки пакетов (dpkg.log)
- По `/var/log/dpkg.log` посчитайте, какие пакеты устанавливались чаще всего (события `install`).
- Выведите TOP‑10 пакетов с количеством установок.

#### Команда (для auth.log)
  ```bash
grep ' install ' /var/log/dpkg.log |
awk '{for (i=1; i<=NF; i++) if ($i == "install") print $(i+1)}' |
sed 's/:.*//' |
sort | uniq -c |
sort -nr |
head -10
  
  ```
#### Результат выполнения команды
```
1 zstd
1 zlib1g
1 zip
1 zfsutils-linux
1 zfs-zed
1 zenity-common
1 zenity
1 yelp-xsl
1 yelp
1 yaru-theme-sound
```
#### Вывод
Можно применять различные регулярные выражения для поиска необходимой информации в логах.

