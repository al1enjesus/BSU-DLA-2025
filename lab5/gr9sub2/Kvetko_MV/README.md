```markdown
# Лабораторная работа №5: Модули ядра Linux

**Студент:** Кветко Матвей  
**Группа:** 9, Подгруппа 2  
**Вариант:** 2  

---

## Введение

Цель работы — изучение разработки модулей ядра Linux. В ходе выполнения были созданы три модуля:

1. **hello_module** — базовый модуль с выводом приветственного сообщения и поддержкой параметров  
2. **proc_module** — модуль с файлом `/proc/my_config`, позволяющим читать и изменять значения  
3. **sys_stats_module** — модуль для отображения системной статистики через `/proc/sys_stats`  

---

## Структура проекта

```
Kvetko_Matvey/
├── README.md             # Инструкции по запуску
├── REPORT.MD             # Подробный отчёт о работе
├── src/                  # Исходные коды модулей
│   ├── hello_module.c
│   ├── proc_module.c
│   ├── sys_stats_module.c
│   └── Makefile
```

---

## Требования

- **Операционная система:** Linux (рекомендуется Ubuntu 22.04/24.04 или аналог)  
- **Ядро:** Linux kernel с установленными headers  
- **Необходимые пакеты:** `build-essential`, `kmod`  
- **Примечание:** рекомендуется запускать в виртуальной машине  

### Установка зависимостей

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) kmod
```

---

## Быстрый старт

### Проверка окружения

```bash
cd src/
make check
```

### Сборка модулей

```bash
make
```

После сборки появятся: `hello_module.ko`, `proc_module.ko`, `sys_stats_module.ko`.

---

## Тестирование

### 1. Hello Module

```bash
# Загрузка без параметра
sudo insmod hello_module.ko
dmesg | tail -5

# Загрузка с параметром
sudo rmmod hello_module
sudo insmod hello_module.ko message="Custom message"
dmesg | tail -5

# Выгрузка модуля
sudo rmmod hello_module
dmesg | tail -5
```

**Ожидаемое поведение:**

* Без параметра: `Hello from Kvetko Matvey module!`
* С параметром: вывод указанного сообщения
* При выгрузке: `Goodbye from Kvetko Matvey module!`

---

### 2. Proc Module

```bash
sudo insmod proc_module.ko

# Проверка наличия файла
ls -la /proc/my_config

# Чтение (по умолчанию)
cat /proc/my_config

# Запись нового значения
echo "New value" > /proc/my_config

# Проверка обновлённого значения
cat /proc/my_config

# Выгрузка модуля
sudo rmmod proc_module

# Файл должен исчезнуть
ls -la /proc/my_config  # Ошибка
```

**Ожидаемое поведение:**

* По умолчанию содержимое: `default`
* После записи: новое значение
* После выгрузки: файл удаляется

---

### 3. System Stats Module

```bash
sudo insmod sys_stats_module.ko

# Просмотр статистики
cat /proc/sys_stats

# Выгрузка
sudo rmmod sys_stats_module
```

**Пример вывода:**

```
Processes: N
Memory Used: XXXX MB
System Uptime: YYYY seconds
```

---

## Очистка

```bash
make clean
```

---

## Работа с модулями

```bash
# Список загруженных модулей
lsmod | grep -E "hello|proc|sys_stats"

# Просмотр информации о модуле
modinfo hello_module.ko
modinfo proc_module.ko
modinfo sys_stats_module.ko

# Просмотр логов ядра
dmesg | tail -20
dmesg -w  # просмотр в реальном времени
```

---

## Особенности реализации

### Hello Module

* Поддержка параметра `message` для пользовательского приветствия
* Вывод сообщений при загрузке и выгрузке

### Proc Module

* Потокобезопасность (mutex для защиты от race conditions)
* Поддержка чтения и записи
* Ограничение буфера: 256 байт
* Автоматическое удаление символа новой строки при записи

### System Stats Module

* Подсчёт процессов в состоянии `TASK_RUNNING`
* Отображение используемой памяти (МБ)
* Время работы системы (секунды)
* Использование RCU lock для безопасной итерации по процессам
```