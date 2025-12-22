# Лабораторная 5 - Модули ядра (Вариант 2) - Быстрый старт

**Автор:** Черноокий Д И  


---

## ⚠️ КРИТИЧЕСКОЕ ПРЕДУПРЕЖДЕНИЕ О БЕЗОПАСНОСТИ

**Эта лабораторная ДОЛЖНА выполняться в виртуальной машине (Fedora)**



**Если нет VM, создайте её СНАЧАЛА:**
- VirtualBox (рекомендуется) - бесплатно
- VMware - платно, но быстрее
- KVM/QEMU - для Linux
- WSL2 - для Windows (осторожно)

---

## Быстрый старт (5 минут)

### 1. Первоначальная настройка (только первый раз)

```bash
# В терминале Fedora VM перейдите в эту директорию:
cd lab5/gr1sub1/Chernookii_DI/

# Сделайте скрипт исполняемым
chmod +x run_lab5.sh

# Установка зависимостей (требует sudo)
./run_lab5.sh setup
```

### 2. Сборка модулей

```bash
./run_lab5.sh build
```

Ожидаемый вывод:
```
Building kernel modules for Variant 2...
Modules: hello_module, config_module, stats_module
✓ Modules built successfully!
```

### 3. Тестирование модулей

```bash
# Протестировать все модули (требует sudo)
sudo ./run_lab5.sh test-all

# Или тестировать отдельно:
sudo ./run_lab5.sh test-hello
sudo ./run_lab5.sh test-config
sudo ./run_lab5.sh test-stats
```

### 4. Просмотр результатов

- **Логи ядра:** `dmesg | tail`
- **Логи тестов:** смотрите в директории `logs/`
- **Исходный код:** смотрите в директории `src/`

---

## Полное использование

```bash
./run_lab5.sh help            # Показать справку
./run_lab5.sh setup           # Установить зависимости
./run_lab5.sh build           # Собрать модули
./run_lab5.sh test-hello      # Протестировать hello_module
./run_lab5.sh test-config     # Протестировать config_module
./run_lab5.sh test-stats      # Протестировать stats_module
./run_lab5.sh test-all        # Протестировать все модули
./run_lab5.sh clean           # Удалить собранные файлы
./run_lab5.sh full-cycle      # Setup + Build + Test
```

---

## Ручные операции (Без скрипта)

### Сборка вручную

```bash
cd src/
make -f ../Makefile check
make -f ../Makefile
```

### Загрузка модулей вручную

```bash
# Требуется sudo
sudo insmod src/hello_module.ko
sudo insmod src/hello_module.ko message="Пользовательское сообщение"

sudo insmod src/config_module.ko
cat /proc/my_config
echo "новое значение" > /proc/my_config

sudo insmod src/stats_module.ko
cat /proc/sys_stats
```

### Выгрузка модулей

```bash
sudo rmmod hello_module
sudo rmmod config_module
sudo rmmod stats_module
```

### Просмотр логов

```bash
# Последние сообщения ядра
dmesg | tail -20

# Мониторинг в реальном времени
dmesg -w

# С временными метками
dmesg -T | tail -20

# Поиск по модулю
dmesg | grep hello_module
```

---

## Решение проблем

### Проблема: "Kernel headers not found"

**Решение:**
```bash
./run_lab5.sh setup

# Или вручную:
sudo dnf install -y kernel-devel kernel-headers
```

### Проблема: "Permission denied" при загрузке модулей

**Решение:** Используйте `sudo`
```bash
sudo insmod module.ko
sudo rmmod module
```

### Проблема: "ERROR: could not insert module: File exists"

**Решение:** Модуль уже загружен. Выгрузите сначала:
```bash
sudo rmmod module_name
```

### Проблема: Система зависла / kernel panic

**Решение:**
1. Принудительное выключение VM (VirtualBox → Machine → Close)
2. Восстановление из снимка или перезагрузка
3. Проверка кода на ошибки
4. Исправление и новая попытка

### Проблема: Не найден /proc/my_config

**Решение:** Убедитесь, что config_module загружен:
```bash
sudo insmod src/config_module.ko
ls -la /proc/my_config
```

---

## Структура директорий

```
lab5/gr1sub1/Chernookii_DI/
├── run_lab5.sh           # Главный скрипт (ЭТОТ ФАЙЛ)
├── Makefile              # Конфигурация сборки
├── REPORT.MD             # Подробный отчет (ЧИТАЙТЕ!)
├── README.MD             # Этот файл
├── src/
│   ├── hello_module.c    # Задание A - Hello World
│   ├── config_module.c   # Задание B - /proc с чтением/записью
│   └── stats_module.c    # Задание C - /proc со статистикой
├── screenshots/          # Для скриншотов (пусто)
└── logs/                 # Логи тестирования (создается скриптом)
```

---

## Описание модулей

### Задание A: hello_module.ko

**Что он делает:**
- Выводит приветствие при загрузке
- Выводит прощание при выгрузке
- Принимает параметр `message`

**Тест:**
```bash
sudo insmod src/hello_module.ko
dmesg | tail -1

sudo insmod src/hello_module.ko message="Привет"
dmesg | tail -1

sudo rmmod hello_module
dmesg | tail -1
```

### Задание B: config_module.ko

**Что он делает:**
- Создает файл `/proc/my_config`
- По умолчанию содержит "default"
- Позволяет читать и писать через `/proc/my_config`

**Тест:**
```bash
sudo insmod src/config_module.ko

cat /proc/my_config              # Читать значение
echo "тест" > /proc/my_config    # Писать значение
cat /proc/my_config              # Читать обновленное

sudo rmmod config_module
```

### Задание C: stats_module.ko

**Что он делает:**
- Создает файл `/proc/sys_stats`
- Выводит статистику системы:
  - Количество запущенных процессов
  - Использованная память (в MB)
  - Общая память (в MB)
  - Свободная память (в MB)
  - Uptime системы в секундах
  - Load average

**Тест:**
```bash
sudo insmod src/stats_module.ko
cat /proc/sys_stats
sleep 2
cat /proc/sys_stats
sudo rmmod stats_module
```

---

## Советы по разработке

### Изменение кода

1. Отредактируйте исходник в `src/`
2. Пересоберите: `./run_lab5.sh build`
3. Протестируйте: `sudo ./run_lab5.sh test-<name>`

### Отладка с dmesg

```bash
# Смотреть логи в реальном времени
dmesg -w

# В другом терминале, загружать/тестировать модули
sudo insmod src/hello_module.ko
```

### Информация о модуле

```bash
modinfo src/hello_module.ko    # До загрузки
cat /sys/module/hello_module/parameters/  # После загрузки
```

---

## Полезные команды

```bash
# Список загруженных модулей
lsmod | grep hello

# Информация о модуле
modinfo src/hello_module.ko

# Просмотр логов ядра
dmesg | tail -20

# Проверка прав (текущий пользователь)
whoami

# Сделать файл исполняемым
chmod +x script.sh

# Просмотр версии ядра
uname -r
```

---

## Стандартный рабочий процесс

### Первая установка
```bash
cd ~/lab5/gr1sub1/Chernookii_DI/
chmod +x run_lab5.sh
./run_lab5.sh setup      # Занимает 5-10 минут
```

### Ежедневная разработка
```bash
./run_lab5.sh build      # Компиляция
sudo ./run_lab5.sh test-all  # Тестирование всех
dmesg | grep "модуль"    # Проверка логов
```

### Перед сдачей
```bash
./run_lab5.sh clean      # Очистка
./run_lab5.sh build      # Свежая сборка
sudo ./run_lab5.sh test-all  # Проверка всех
cat REPORT.MD            # Просмотр отчета
```

---

## Дополнительно: Если что-то сломалось

### Система зависла (kernel panic)

1. **Принудительное выключение:**
   - VirtualBox: Правый клик на VM → Close → Power off
   - Или: Ctrl+Alt+SysRq → R → E → I → S → U → B

2. **Восстановление из снимка:**
   - VirtualBox: Machine → Snapshots → Restore

3. **Загрузка и проверка:**
   - Попробуйте загрузить другой модуль
   - Проверьте dmesg на ошибки

### Модуль не загружается

```bash
# Подробная информация об ошибке
sudo insmod src/module.ko 2>&1

# Проверка совместимости версии ядра
modinfo src/module.ko
uname -r

# Проверка, не загружен ли уже
lsmod | grep module

# Попробуйте выгрузить сначала
sudo rmmod module 2>/dev/null || true
```

### /proc файл не появляется

```bash
# Проверить, загружен ли модуль
lsmod | grep config_module

# Проверить сообщения ядра
dmesg | tail -10

# Попробовать загрузить снова с verbose
sudo insmod src/config_module.ko
dmesg | grep config_module

# Проверить /proc
ls -la /proc/my* /proc/sys_*
```

---

## Примечания о производительности

- **Время сборки:** 1-2 минуты
- **Загрузка модуля:** < 1 секунды
- **Первый полный цикл:** 10-15 минут (с установкой)

---

## Дополнительные ресурсы

- **Документация ядра:** `/usr/src/linux-headers-$(uname -r)/`
- **Man страницы:** `man insmod`, `man rmmod`, `man dmesg`
- **Онлайн гайды:**
  - kernel.org документация
  - LWN.net статьи про ядро
  - The Linux Kernel Module Programming Guide

---

## Быстрая проверка

- [ ] Работаете в виртуальной машине?
- [ ] Сделали снимок (snapshot) перед началом?
- [ ] Установлены kernel headers? (`./run_lab5.sh setup`)
- [ ] Модули собраны? (`./run_lab5.sh build`)
- [ ] Можете загрузить hello модуль? (`sudo ./run_lab5.sh test-hello`)
- [ ] Можете читать/писать config? (`sudo ./run_lab5.sh test-config`)
- [ ] Можете читать stats? (`sudo ./run_lab5.sh test-stats`)
- [ ] Отчет заполнен? (Читайте `REPORT.MD`)

---

**Нужна помощь?**

1. Проверьте логи: `cat logs/*.log`
2. Прочитайте подробный отчет: `cat REPORT.MD`
3. Посмотрите сообщения ядра: `dmesg | tail -50`
4. Запустите: `./run_lab5.sh help`

---

**Удачи! И помните: Всегда работайте в VM!** 🐧
