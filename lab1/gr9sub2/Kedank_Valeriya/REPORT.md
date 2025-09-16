## Задание A

# Условие

Частотный анализ слов в syslog (TOP‑5):
  Подсчитайте 5 самых частых “слов” в /var/log/syslog (нормализуйте регистр, отделяйте слова).

# Команда выполнения

```
     cat syslog \
>   | tr -cs '[:alnum:]' '\n' \
>   | tr '[:upper:]' '[:lower:]' \
>   | sort \
>   | uniq -c \
>   | sort -nr \
>   | head -n 5
```

# Результат
  44825 lera
  44708 x513ean
  44650 vivobook
  44650 k513ea
  44650 asuslaptop


## Задание Б

# Условие

Неудачные попытки входа (auth.log):

   - Найдите строки с Failed или Invalid в /var/log/auth.log.
   - Извлеките IP‑адреса, посчитайте TOP‑10 источников.
   - Замаскируйте последний октет IP (например, 192.168.0.15 → 192.168.0.x) с помощью sed.
     Подсказка: используйте grep -E, awk/cut, sort | uniq -c | sort -nr, и sed -E 's/(\d+\.\d+\.\d+\.)\d+/\1x/g'
     
# Пример выполнения (в auth.log не нашлось ошибок, для примера предоставила syslog):

```
lera@lera-VivoBook-ASUSLaptop-X513EAN-K513EA:/var/log$ grep -E 'Failed|Invalid' auth.log
lera@lera-VivoBook-ASUSLaptop-X513EAN-K513EA:/var/log$ grep -E 'Failed|Invalid' syslog
Aug 25 13:22:21 lera-VivoBook-ASUSLaptop-X513EAN-K513EA kernel: [    2.246660] usb 1-6: Failed to initialize entity for entity 5
Aug 25 13:22:21 lera-VivoBook-ASUSLaptop-X513EAN-K513EA kernel: [    2.246664] usb 1-6: Failed to register entities (-22).
Aug 25 13:22:21 lera-VivoBook-ASUSLaptop-X513EAN-K513EA kernel: [    2.399659] i915 0000:00:02.0: [drm] Failed to load DMC firmware i915/tgl_dmc_ver2_12.bin. Disabling runtime power management.
Aug 25 13:22:22 lera-VivoBook-ASUSLaptop-X513EAN-K513EA udisksd[940]: Failed to load the 'mdraid' libblockdev plugin
Aug 25 13:22:23 lera-VivoBook-ASUSLaptop-X513EAN-K513EA pulseaudio[1109]: Failed to open cookie file '/home/lera/.config/pulse/cookie': Нет такого файла или каталога
Aug 25 13:22:23 lera-VivoBook-ASUSLaptop-X513EAN-K513EA pulseaudio[1109]: Failed to load authentication key '/home/lera/.config/pulse/cookie': Нет такого файла или каталога
Aug 25 13:22:23 lera-VivoBook-ASUSLaptop-X513EAN-K513EA pulseaudio[1109]: Failed to open cookie file '/home/lera/.pulse-cookie': Нет такого файла или каталога
Aug 25 13:22:23 lera-VivoBook-ASUSLaptop-X513EAN-K513EA pulseaudio[1109]: Failed to load authentication key '/home/lera/.pulse-cookie': Нет такого файла или каталога
Aug 25 13:22:24 lera-VivoBook-ASUSLaptop-X513EAN-K513EA gsd-media-keys[1565]: Failed to grab accelerator for keybinding settings:playback-random
Aug 25 13:22:24 lera-VivoBook-ASUSLaptop-X513EAN-K513EA gsd-media-keys[1565]: Failed to grab accelerator for keybinding settings:hibernate
```


## Задание В

# Условие

Установки пакетов (dpkg.log)

    По /var/log/dpkg.log посчитайте, какие пакеты устанавливались чаще всего (события install).
    Выведите TOP‑10 пакетов с количеством установок.

# Команда выполнения

```
lera@lera-VivoBook-ASUSLaptop-X513EAN-K513EA:/var/log$ grep ' install ' dpkg.log | tr -cs '[:alnum:]' '\n' \
>   | tr '[:upper:]' '[:lower:]' \
>   | sort \
>   | uniq -c \
>   | sort -nr \
>   | head -n 10
```

# Результат выполнения

   1867 23
   1865 08
   1856 02
   1854 install
   1849 2022
   1826 none
   1780 1
   1288 amd64
   1063 2
    999 0

