# ИНСТРУКЦИИ ПО ИСПОЛЬЗОВАНИЮ ЛАБОРАТОРНОЙ РАБОТЫ 5

**Автор:** Черноокий Д И  


---

##  КРИТИЧЕСКОЕ ПРЕДУПРЕЖДЕНИЕ

**ЭТА ЛАБОРАТОРНАЯ РАБОТА ДОЛЖНА ВЫПОЛНЯТЬСЯ В ВИРТУАЛЬНОЙ МАШИНЕ!**

## СТРУКТУРА ПРОЕКТА

```
lab5/gr1sub1/Chernookii_DI/
├── fedora_full_setup.sh      ← ГЛАВНЫЙ СКРИПТ
├── run_lab5.sh               ← Альтернативный скрипт для тестирования
├── Makefile                  ← Для сборки модулей
├── REPORT.MD                 ← ПОДРОБНЫЙ ОТЧЕТ 
├── README.md                 ← Краткий гайд
├── SETUP_INSTRUCTIONS.md     ← Этот файл
├── src/
│   ├── hello_module.c        ← Задание A
│   ├── config_module.c       ← Задание B
│   └── stats_module.c        ← Задание C
├── logs/                     ← Логи тестирования (создается автоматически)
└── screenshots/              ← Скриншоты 
```

---

## БЫСТРЫЙ СТАРТ (5 МИНУТ)

### Шаг 1: Откройте терминал в Fedora VM

```bash
# Перейдите в рабочую директорию
cd ~/BSU-DLA-2025/lab5/gr1sub1/Chernookii_Di/

# Или если находитесь в другой директории:
cd /path/to/lab5/gr1sub1/Chernookii_Di/
```

### Шаг 2: Запустите главный скрипт (ВСЕ ВКЛЮЧЕНО!)

```bash
./fedora_full_setup.sh
```

**Этот скрипт автоматически:**
1. ✅ Проверит, что вы в виртуальной машине
2. ✅ Установит все необходимые зависимости
3. ✅ Соберет все модули
4. ✅ Протестирует каждый модуль
5. ✅ Покажет результаты

**Ожидаемое время:** 10-15 минут

### Шаг 3: Просмотрите результаты

```bash
# Прочитайте подробный отчет
cat REPORT.MD

# Проверьте логи тестирования
ls -lh logs/

# Просмотрите сводку настройки
cat SETUP_SUMMARY.txt
```

---

## АЛЬТЕРНАТИВНЫЙ МЕТОД: Пошаговый (Если скрипт не работает)

### Вариант A: С использованием run_lab5.sh

```bash
# 1. Установка зависимостей
./run_lab5.sh setup

# 2. Сборка модулей
./run_lab5.sh build

# 3. Тестирование
sudo ./run_lab5.sh test-all

# Или тестируйте по отдельности:
sudo ./run_lab5.sh test-hello
sudo ./run_lab5.sh test-config
sudo ./run_lab5.sh test-stats
```

### Вариант B: Вручную

```bash
# 1. Установка зависимостей
sudo dnf update -y
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y kernel-devel kernel-headers

# 2. Проверка окружения
make check

# 3. Сборка
make clean
make

# 4. Тестирование hello_module
sudo insmod src/hello_module.ko
dmesg | tail -5
sudo rmmod hello_module

# 5. Тестирование config_module
sudo insmod src/config_module.ko
cat /proc/my_config
echo "test" > /proc/my_config
cat /proc/my_config
sudo rmmod config_module

# 6. Тестирование stats_module
sudo insmod src/stats_module.ko
cat /proc/sys_stats
sudo rmmod stats_module
```

---

## ДЕТАЛЬНОЕ ОПИСАНИЕ МОДУЛЕЙ

### Задание A: hello_module.ko

**Функциональность:**
- При загрузке выводит приветствие
- При выгрузке выводит прощание
- Поддерживает параметр `message`

**Тест:**
```bash
# Без параметра (дефолтное сообщение)
sudo insmod src/hello_module.ko
dmesg | tail -1
sudo rmmod hello_module

# С параметром
sudo insmod src/hello_module.ko message="Привет из модуля!"
dmesg | tail -1
sudo rmmod hello_module
```

**Ожидаемый вывод:**
```
hello_module: Hello from Chernookii D.I. module!
hello_module: Привет из модуля!
hello_module: Goodbye from Chernookii D.I. module!
```

---

### Задание B: config_module.ko

**Функциональность:**
- Создает файл `/proc/my_config`
- Позволяет читать и писать конфигурацию
- По умолчанию содержит "default"
- Максимум 256 символов

**Тест:**
```bash
# Загрузка
sudo insmod src/config_module.ko

# Чтение (должно быть "default")
cat /proc/my_config

# Запись
echo "моя конфигурация" > /proc/my_config

# Чтение обновленного значения
cat /proc/my_config

# Выгрузка
sudo rmmod config_module
```

**Ожидаемый результат:**
```
$ cat /proc/my_config
default

$ echo "test" > /proc/my_config

$ cat /proc/my_config
test
```

---

### Задание C: stats_module.ko

**Функциональность:**
- Создает файл `/proc/sys_stats`
- Выводит статистику системы:
  - Количество запущенных процессов
  - Используемая память
  - Общая память
  - Свободная память
  - Uptime системы
  - Load average

**Тест:**
```bash
# Загрузка
sudo insmod src/stats_module.ko

# Просмотр статистики
cat /proc/sys_stats

# Еще раз (должна обновиться)
cat /proc/sys_stats

# Выгрузка
sudo rmmod stats_module
```

**Ожидаемый вывод:**
```
=== System Statistics ===
Processes running: 156
Memory Used: 2048 MB
Memory Total: 7976 MB
Memory Free: 5928 MB
System Uptime: 34567 seconds
Load Average: 0.45
================================
```

---

## ДОСТУПНЫЕ КОМАНДЫ

### С использованием fedora_full_setup.sh

```bash
./fedora_full_setup.sh
```
Полный цикл: установка + сборка + тестирование

### С использованием run_lab5.sh

```bash
./run_lab5.sh help              # Справка
./run_lab5.sh setup             # Установить зависимости
./run_lab5.sh build             # Собрать модули
./run_lab5.sh test-hello        # Тестировать Task A
./run_lab5.sh test-config       # Тестировать Task B
./run_lab5.sh test-stats        # Тестировать Task C
./run_lab5.sh test-all          # Тестировать все
./run_lab5.sh clean             # Удалить собранные файлы
./run_lab5.sh full-cycle        # Setup + Build + Test
```

### Команды для просмотра

```bash
# Просмотр логов ядра
dmesg | tail -20
dmesg -w                          # В реальном времени

# Список загруженных модулей
lsmod
lsmod | grep hello_module

# Информация о модуле
modinfo src/hello_module.ko

# Проверка /proc файлов
cat /proc/my_config
cat /proc/sys_stats
ls -la /proc/my_*
```

---

## РЕШЕНИЕ ПРОБЛЕМ

### Проблема 1: "kernel headers not found"

**Решение:**
```bash
./fedora_full_setup.sh setup

# Или вручную:
sudo dnf install -y kernel-devel kernel-headers
```

### Проблема 2: "Permission denied" при загрузке модуля

**Решение:**
```bash
# Используйте sudo
sudo insmod src/module.ko
sudo rmmod module
```

### Проблема 3: "Module not found" при тестировании

**Решение:**
```bash
# Сначала соберите модули
./run_lab5.sh build

# Проверьте, что файлы созданы
ls -la src/*.ko
```

### Проблема 4: Система зависла / kernel panic

**Решение:**
1. **Force Power Off VM**
   - VirtualBox: нажмите Ctrl+Alt+Delete и выберите Shut Down
   - Или нажмите и держите кнопку питания

2. **Восстановите из snapshot**
   - VirtualBox: Machine → Snapshots → Restore

3. **Найдите ошибку в коде**
   - Проверьте `dmesg` перед паникой

4. **Исправьте и попробуйте снова**

### Проблема 5: "/proc/my_config не найден"

**Решение:**
```bash
# Проверьте, что модуль загружен
lsmod | grep config_module

# Если не загружен, загрузите:
sudo insmod src/config_module.ko

# Проверьте файл:
ls -la /proc/my_config
cat /proc/my_config
```

### Проблема 6: "Module is in use"

**Решение:**
```bash
# Некоторые процессы используют модуль
# Попробуйте позже:
sleep 5
sudo rmmod config_module

# Или проверьте, что использует:
lsof /proc/my_config  # (если доступно)
```

---

## ДОКУМЕНТИРОВАНИЕ (Screenshots и Logs)

### Автоматическое (все скрипты)
Все скрипты создают логи в директории `logs/`:
```bash
ls -la logs/
cat logs/hello_test_*.log
```

### Ручное документирование
```bash
# Сохраните вывод dmesg
dmesg > screenshots/dmesg_$(date +%Y%m%d_%H%M%S).txt

# Сохраните список модулей
lsmod > screenshots/lsmod_$(date +%Y%m%d_%H%M%S).txt

# Сохраните содержимое /proc файлов
cat /proc/my_config > screenshots/config_$(date +%Y%m%d_%H%M%S).txt
cat /proc/sys_stats > screenshots/stats_$(date +%Y%m%d_%H%M%S).txt
```

---

## ТИПОВОЙ РАБОЧИЙ ПРОЦЕСС

### День 1: Настройка

```bash
cd lab5/gr1sub1/Chernookii_DI/
chmod +x *.sh
./fedora_full_setup.sh
# Ждем 10-15 минут
```

### День 2: Разработка (если нужны изменения)

```bash
# Отредактируйте исходный файл (например src/hello_module.c)
nano src/hello_module.c

# Пересоберите
./run_lab5.sh build

# Протестируйте
sudo ./run_lab5.sh test-hello

# Просмотрите логи
dmesg | grep hello_module
```

### День 3: Финализация

```bash
# Убедитесь, что все работает
sudo ./run_lab5.sh test-all

# Просмотрите отчет
cat REPORT.MD

# Проверьте логи
cat logs/*.log

# Очистите (опционально)
./run_lab5.sh clean

# Повторно соберите для проверки
./run_lab5.sh build
sudo ./run_lab5.sh test-all
```

---

## ВАЖНЫЕ ФАЙЛЫ

| Файл | Описание |
|------|---------|
| `fedora_full_setup.sh` | Главный скрипт - ВСЕ В ОДНОМ |
| `run_lab5.sh` | Альтернативный скрипт с опциями |
| `Makefile` | Конфигурация сборки модулей |
| `REPORT.MD` | Полный подробный отчет |
| `README.md` | Краткий гайд |
| `src/hello_module.c` | Исходник Task A |
| `src/config_module.c` | Исходник Task B |
| `src/stats_module.c` | Исходник Task C |
| `logs/` | Директория с логами (создается) |
| `screenshots/` | Директория для скриншотов |

---

## ПРОСМОТР РЕЗУЛЬТАТОВ

### Итоговый отчет
```bash
cat REPORT.MD | less
```

### Быстрая информация
```bash
cat README.md | less
```

### Логи тестирования
```bash
ls -lh logs/
cat logs/hello_test_*.log
```

### Сводка настройки
```bash
cat SETUP_SUMMARY.txt
```

---


## БЫСТРЫЕ КОМАНДЫ

```bash
# Все в одном
./fedora_full_setup.sh

# Помощь
./run_lab5.sh help

# Сборка
./run_lab5.sh build

# Тестирование (требует sudo)
sudo ./run_lab5.sh test-all

# Просмотр логов ядра
dmesg | tail -20
dmesg -w

# Проверка модулей
lsmod | grep -E "hello|config|stats"

# Очистка
./run_lab5.sh clean

# Подробный отчет
less REPORT.MD

# Краткая справка
less README.md
```


---

