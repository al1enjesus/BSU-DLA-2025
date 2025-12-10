# Lab 5 - Быстрый старт (ВАЖНО: ПРОЧИТАЙТЕ ПОЛНОСТЬЮ!)

##  КРИТИЧЕСКИ ВАЖНО - БЕЗОПАСНОСТЬ ПРЕВЫШЕ ВСЕГО! 

###  Эта лабораторная ОБЯЗАТЕЛЬНО выполняется в виртуальной машине!

**ПОЧЕМУ ЭТО ТАК ВАЖНО:**

Модули ядра выполняются в **kernel space** - привилегированном режиме с полным доступом ко всей системе. Ошибка в коде модуля может привести к:

- **Kernel Panic** - полный крах системы
- **Потеря данных** - если не успели сохранить работу
- **Нестабильность** - система может работать неправильно
- **Невозможность загрузки** - в худшем случае

### Безопасные варианты окружения:

1. **VirtualBox** (рекомендуется для начинающих)
   - Бесплатно
   - Легко установить
   - Можно сделать snapshot

2. **VMware Workstation/Fusion** (если есть лицензия)
   - Более производительно
   - Snapshot функция

3. **Docker с --privileged** (для продвинутых)
   - Быстрый старт
   - Но требует понимания Docker

### НЕ ДЕЛАЙТЕ НА:

- Основной системе (ваш рабочий компьютер)
- Ноутбуке с важными данными
- Сервере в продакшене (!!!!)
- macOS напрямую (ядро другое)

---

## 1. Подготовка окружения (15-20 минут)

### Вариант A: VirtualBox + Ubuntu (рекомендуется)

**Шаг 1: Установите VirtualBox**
```bash
# На хост-системе
# Скачайте с https://www.virtualbox.org/
# Или через пакетный менеджер
```

**Шаг 2: Создайте VM с Ubuntu**
- Скачайте Ubuntu 22.04 или 24.04 ISO
- Создайте VM:
  - RAM: минимум 2 GB (рекомендуется 4 GB)
  - Disk: 20 GB
  - CPU: 2 cores
- Установите Ubuntu

**Шаг 3: Сделайте snapshot "Clean Install"**
```
VirtualBox → Выберите VM → Snapshots → Take Snapshot
Имя: "Clean Ubuntu Install"
```

**Зачем:** Если что-то сломается, вернётесь к этой точке за 10 секунд!

**Шаг 4: Установите необходимые пакеты (внутри VM)**
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) \
     kmod vim git dkms

# Проверка
uname -r                         # Версия ядра
ls /lib/modules/$(uname -r)/build  # Kernel headers
gcc --version                    # Компилятор
```

**Шаг 5: Сделайте второй snapshot "Ready for Development"**
```
VirtualBox → Snapshots → Take Snapshot
Имя: "Ready for Development"
```

---

### Вариант B: Docker (для опытных пользователей)

```bash
# Создать контейнер с привилегиями
docker run -it --privileged \
    --name kernel-lab \
    -v $(pwd):/workspace \
    ubuntu:22.04 bash

# Внутри контейнера
apt update
apt install -y build-essential linux-headers-generic kmod vim

# Проверка
uname -r
ls /lib/modules/$(uname -r)/build
```

**Предупреждение:** Docker контейнер использует ядро хост-системы! Kernel panic в контейнере = kernel panic хоста!

---

## 2. Первые шаги (10 минут)

### Клонируйте репозиторий (внутри VM)

```bash
git clone <URL_РЕПОЗИТОРИЯ>
cd BSU-OS-2025/lab5/samples/

# Проверьте окружение
make check
```

**Ожидаемый вывод:**
```
=== Environment Check ===
Kernel version: 5.15.0-...-generic
Kernel headers: ✓ Found at /lib/modules/.../build
Build tools: ✓ gcc installed
Module loading: ✓ insmod available
```

Если видите ✗ - установите недостающие пакеты!

---

### Соберите примеры

```bash
make
```

**Ожидаемый вывод:**
```
Building kernel modules...
make[1]: Entering directory '/usr/src/linux-headers-...'
  CC [M]  /path/to/lab5/samples/hello_module.o
  CC [M]  /path/to/lab5/samples/proc_module.o
  CC [M]  /path/to/lab5/samples/chardev_module.o
  ...
  LD [M]  /path/to/lab5/samples/hello_module.ko
  LD [M]  /path/to/lab5/samples/proc_module.ko
  LD [M]  /path/to/lab5/samples/chardev_module.ko
✓ Modules built successfully!
```

**Если ошибки компиляции:**
1. Проверьте `make check`
2. Убедитесь, что kernel headers установлены
3. Проверьте версию gcc (должна быть совместима с ядром)

---

### Первый запуск модуля

```bash
# Загрузить модуль
sudo insmod hello_module.ko

# Посмотреть логи
dmesg | tail -5

# Проверить, что модуль загружен
lsmod | grep hello_module

# Выгрузить модуль
sudo rmmod hello_module

# Снова посмотреть логи
dmesg | tail -5
```

**Ожидаемый вывод dmesg:**
```
[12345.678] hello_module: Module loaded (TODO: implement greeting)
[12350.123] hello_module: Module unloaded (TODO: implement goodbye)
```

**Если видите свои сообщения - поздравляю, всё работает!** 🎉

---

## 3. Что делать дальше (порядок выполнения)

### Шаг 1: Определите свой вариант

Откройте [README.md](README.md) и найдите раздел "Правило выбора варианта":
- **Нечётный номер по списку** → Вариант 1
- **Чётный номер по списку** → Вариант 2

### Шаг 2: Создайте свою папку

```bash
cd ../../  # В корень lab5/
mkdir -p gr<группа>sub<подгруппа>/ФАМИЛИЯ_ИМЯ/src
cd gr<группа>sub<подгруппа>/ФАМИЛИЯ_ИМЯ/

# Создать базовые файлы
touch REPORT.MD
cp ../../samples/Makefile .
```

### Шаг 3: Скопируйте скелеты

```bash
cd src/
cp ../../../samples/hello_module.c .
cp ../../../samples/proc_module.c .
cp ../../../samples/chardev_module.c .
```

### Шаг 4: Реализуйте задания по порядку

**Задание A: Hello World** (30 минут)
1. Откройте `hello_module.c`
2. Найдите все `// TODO:`
3. Реализуйте функции
4. Скомпилируйте: `make`
5. Протестируйте:
   ```bash
   sudo insmod hello_module.ko
   dmesg | tail -5
   sudo insmod hello_module.ko message="Test"
   dmesg | tail -5
   sudo rmmod hello_module
   ```

**Задание B: /proc файл** (1-2 часа)
1. Откройте `proc_module.c`
2. Реализуйте TODO
3. Протестируйте:
   ```bash
   sudo insmod proc_module.ko
   cat /proc/student_info
   cat /proc/student_info  # Счётчик должен увеличиться!
   sudo rmmod proc_module
   ```

**Задание C: Character Device** (2-3 часа)
1. Откройте `chardev_module.c`
2. Реализуйте TODO
3. Протестируйте (см. комментарии в файле)

---

## 4. Частые проблемы и решения

### Проблема: "ERROR: could not insert module: Invalid module format"

**Причина:** Модуль собран для другой версии ядра

**Решение:**
```bash
make clean
make
# Убедитесь, что используете правильные kernel headers
ls /lib/modules/$(uname -r)/build
```

---

### Проблема: "ERROR: could not insert module: Operation not permitted"

**Причина 1:** Нет прав

**Решение:**
```bash
sudo insmod module.ko  # Нужны права root
```

**Причина 2:** Secure Boot

**Решение:**
```bash
# Проверить статус Secure Boot
mokutil --sb-state

# Если включен, либо отключите в BIOS, либо подпишите модуль
# (Для учебных целей проще отключить Secure Boot в VM)
```

---

### Проблема: Kernel Panic!

**Что делать:**
1. **Не паникуйте!** (каламбур intended)
2. Если VM зависла - Force Power Off через VirtualBox
3. Восстановите snapshot "Ready for Development"
4. Проверьте код на ошибки:
   - Обращение к NULL указателям?
   - Выход за границы массива?
   - Забыли copy_to_user/copy_from_user?
5. Добавьте больше printk для отладки
6. Попробуйте снова

**Профилактика:**
- Делайте snapshot перед каждым экспериментом
- Проверяйте все указатели на NULL
- Используйте copy_to_user/copy_from_user
- Не делайте бесконечные циклы

---

### Проблема: "Module is in use"

**Причина:** Кто-то использует модуль (файл открыт, устройство используется)

**Решение:**
```bash
# Закройте все программы, использующие модуль
# Для character device:
sudo rm /dev/mychardev  # Сначала удалите device node
sudo rmmod chardev_module

# Для proc файла:
# Убедитесь, что никто не читает /proc файл
sudo rmmod proc_module
```

---

### Проблема: Ничего не видно в dmesg

**Причина:** Уровень логирования фильтрует сообщения

**Решение:**
```bash
# Посмотреть текущий уровень
cat /proc/sys/kernel/printk
# Первое число - текущий уровень (7 = все сообщения)

# Установить максимальный уровень
echo 7 > /proc/sys/kernel/printk

# Или использовать dmesg с опциями
dmesg --level=info,notice,warn,err
```

---

## 5. Отладка модулей

### Техники отладки:

**1. printk - ваш лучший друг**
```c
printk(KERN_DEBUG "Entering function %s\n", __func__);
printk(KERN_DEBUG "Value: %d, Pointer: %p\n", val, ptr);
printk(KERN_DEBUG "About to call risky function\n");
risky_function();
printk(KERN_DEBUG "Survived risky function\n");
```

**2. Проверка каждого шага**
```c
if (!ptr) {
    printk(KERN_ERR "Pointer is NULL!\n");
    return -EINVAL;
}
printk(KERN_DEBUG "Pointer is valid: %p\n", ptr);
```

**3. Мониторинг в реальном времени**
```bash
# В одном терминале
dmesg -w

# В другом терминале
sudo insmod module.ko
# Сразу видите вывод!
```

**4. Проверка утечек памяти**
```bash
# До загрузки
cat /proc/meminfo | grep Slab

# Загрузить/выгрузить модуль несколько раз
sudo insmod module.ko
sudo rmmod module
sudo insmod module.ko
sudo rmmod module

# После выгрузки
cat /proc/meminfo | grep Slab
# Slab не должен расти
```

---

## 6. Чеклист перед сдачей

### Код:
- [ ] Все TODO реализованы
- [ ] Код компилируется без ошибок и предупреждений
- [ ] Модуль загружается: `sudo insmod`
- [ ] Модуль корректно работает (выполняет требования задания)
- [ ] Модуль выгружается: `sudo rmmod`
- [ ] Нет утечек памяти
- [ ] Заполнены метаданные (MODULE_AUTHOR, MODULE_DESCRIPTION)

### Тестирование:
- [ ] Загрузка/выгрузка 3+ раз подряд (должно работать каждый раз)
- [ ] Проверены все сценарии использования
- [ ] Логи в dmesg понятны и информативны
- [ ] Нет kernel panic
- [ ] Система стабильна после работы с модулем

### Отчёт:
- [ ] Создан REPORT.MD
- [ ] Описаны все задания
- [ ] Приложены скриншоты dmesg, lsmod, тестов
- [ ] Даны ответы на все 30 вопросов
- [ ] Описаны трудности и способы решения
- [ ] Указано, как пользовались AI (если пользовались)

---

## 7. Полезные команды (шпаргалка)

### Управление модулями:
```bash
# Загрузка
sudo insmod module.ko
sudo insmod module.ko param1=value param2=value

# Выгрузка
sudo rmmod module_name

# Список загруженных модулей
lsmod
lsmod | grep my_module

# Информация о модуле
modinfo module.ko
modinfo -p module.ko  # Только параметры
```

### Логи:
```bash
# Последние сообщения
dmesg | tail -20
dmesg | grep module_name

# В реальном времени
dmesg -w

# С человекочитаемым временем
dmesg -T

# Очистить логи (для тестов)
sudo dmesg -C
```

### Для proc файлов:
```bash
# Чтение
cat /proc/myfile

# Запись (если поддерживается)
echo "value" > /proc/myfile

# Проверка, что файл существует
ls -la /proc/myfile
```

### Для character devices:
```bash
# Создать device node
sudo mknod /dev/mydevice c <MAJOR> <MINOR>

# Права доступа
sudo chmod 666 /dev/mydevice

# Запись
echo "data" > /dev/mydevice

# Чтение
cat /dev/mydevice

# Удаление
sudo rm /dev/mydevice
```

### Отладка:
```bash
# Проверка tainted статуса
cat /proc/sys/kernel/tainted

# Информация о системе
uname -a
cat /proc/version

# Проверка модулей в памяти
cat /proc/modules
```

---

## 8. Когда обращаться за помощью

### Самостоятельно можно решить:
- Ошибки компиляции (читайте сообщения!)
- "Module not found" (проверьте путь)
- "Permission denied" (используйте sudo)
- Модуль не работает (добавьте printk, отладьте)

### Стоит спросить:
- Kernel panic при непонятных обстоятельствах
- Модуль загружается, но ничего не делает
- Странное поведение системы после модуля
- Концептуальные вопросы (как работает X?)

### Где спрашивать:
- Issues в репозитории
- Комментарии в PR
- У преподавателя/ассистентов

**При вопросе приложите:**
- Код модуля
- Вывод dmesg
- Команды, которые выполняли
- Что ожидали vs что получили

---

## 9. Важные напоминания

### ✅ DO (Делать):
- Работать в VM
- Делать snapshots
- Проверять все указатели
- Использовать printk для отладки
- Читать dmesg после каждой операции
- Освобождать ресурсы в module_exit
- Тестировать загрузку/выгрузку несколько раз

### ❌ DON'T (Не делать):
- Работать на основной системе
- Игнорировать предупреждения компилятора
- Использовать printf (только printk!)
- Обращаться к user-space указателям напрямую
- Делать бесконечные циклы
- Забывать освобождать память
- Копировать код без понимания

---

## 10. Ресурсы для изучения

### Документация:
- `man insmod`, `man rmmod`, `man lsmod`
- Kernel docs: https://www.kernel.org/doc/html/latest/
- Linux Driver Tutorial: https://sysprog21.github.io/lkmpg/

### Отладка:
- `dmesg --help`
- `/usr/src/linux-headers-$(uname -r)/Documentation/`

### В коде:
- Комментарии в скелетах (там подробные объяснения!)
- Основной [README.md](README.md) (детальная теория)

---

## Итоговые советы

1. **Не спешите.** Kernel-space программирование требует внимательности.

2. **Читайте ошибки.** Компилятор и ядро подсказывают, что не так.

3. **Тестируйте часто.** После каждого изменения - компиляция и тест.

4. **Используйте snapshots.** Это ваша страховка.

5. **Не бойтесь kernel panic.** Это не страшно в VM. Восстановили snapshot и идём дальше.

6. **Задавайте вопросы.** Лучше спросить, чем часами искать проблему.

---

**Время на лабораторную:** 4-8 часов (базовая часть)

**Начинайте прямо сейчас!** 🚀

**И помните: всегда в VM!** 🖥️💾
