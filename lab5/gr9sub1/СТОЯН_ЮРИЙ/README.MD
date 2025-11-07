# Лабораторная работа №5: Модули ядра Linux

**Студент:** Стоян Юрий  
**Группа:** 9, Подгруппа: 1  
**Вариант:** 1 (нечётный номер по списку)

СТОЯН_ЮРИЙ/
├── README.md              # Этот файл
├── REPORT.MD
└── Makefile              # Подробный отчёт о выполнении
├── src/                   # Исходные коды модулей
│   ├── hello_module.c
│   ├── proc_module.c
│   ├── char_device.c
└── screenshots/           # Скриншоты выполнения

## Требования

- **ОС:** Linux (Ubuntu 22.04/24.04 или аналог)
- **Ядро:** Linux kernel с headers (проверьте: `ls /lib/modules/$(uname -r)/build`)
- **Инструменты:** `build-essential`, `kmod`
- **Окружение:** **Обязательно виртуальная машина!**
### Установка зависимостей

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) kmod
```

---

## Быстрый запуск

### 1. Проверка окружения

```bash
cd src/
make check
```

Вы должны увидеть сообщения о том, что kernel headers, gcc и kmod найдены.

### 2. Сборка модулей

```bash
make
```

Будут созданы файлы: `hello_module.ko`, `proc_module.ko`, `char_device.ko`
Команды записаны в REPORT.MD