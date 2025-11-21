 
#!/bin/bash
#
# test_modules.sh - Скрипт для автоматического тестирования модулей ядра
# Лабораторная работа №5 - Вариант 2
#

set -e  # Прерывать при ошибках

COLOR_GREEN='\033[0;32m'
COLOR_RED='\033[0;31m'
COLOR_YELLOW='\033[1;33m'
COLOR_BLUE='\033[0;34m'
COLOR_NC='\033[0m' # No Color

print_header() {
    echo -e "${COLOR_BLUE}===================================${COLOR_NC}"
    echo -e "${COLOR_BLUE}$1${COLOR_NC}"
    echo -e "${COLOR_BLUE}===================================${COLOR_NC}"
}

print_success() {
    echo -e "${COLOR_GREEN}$1${COLOR_NC}"
}

print_error() {
    echo -e "${COLOR_RED}$1${COLOR_NC}"
}

print_warning() {
    echo -e "${COLOR_YELLOW}$1${COLOR_NC}"
}

# Функция для очистки (выгрузка всех модулей)
cleanup() {
    print_header "Очистка перед тестами"
    rmmod hello_module 2>/dev/null || true
    rmmod proc_config_module 2>/dev/null || true
    rmmod proc_stats_module 2>/dev/null || true
    dmesg -C  # Очистка dmesg для чистых логов
    print_success "Все модули выгружены, dmesg очищен"
    echo ""
}

# Тест 1: Hello Module (Задание A)
test_hello_module() {
    print_header "ТЕСТ 1: Hello Module (Задание A)"
    
    # Тест 1.1: Загрузка без параметров
    echo "1.1 Загрузка без параметров..."
    insmod hello_module.ko
    sleep 0.5
    
    if lsmod | grep -q hello_module; then
        print_success "Модуль загружен"
    else
        print_error "Модуль не загружен!"
        return 1
    fi
    
    echo "Логи из dmesg:"
    dmesg | tail -3
    echo ""
    
    # Тест 1.2: Выгрузка
    echo "1.2 Выгрузка модуля..."
    rmmod hello_module
    sleep 0.5
    
    if ! lsmod | grep -q hello_module; then
        print_success "Модуль выгружен"
    else
        print_error "Модуль всё ещё загружен!"
        return 1
    fi
    
    echo "Логи из dmesg:"
    dmesg | tail -3
    echo ""
    
    # Тест 1.3: Загрузка с параметром
    echo "1.3 Загрузка с параметром message='Test-Message'..."
    dmesg -C
    insmod hello_module.ko message="Test-Message"
    sleep 0.5
    
    echo "Логи из dmesg:"
    dmesg | tail -3
    
    rmmod hello_module
    print_success "Тест Hello Module завершён"
    echo ""
}


# Тест 2: Proc Config Module (Задание B)
test_proc_config() {
    print_header "ТЕСТ 2: Proc Config Module (Задание B)"
    
    # Тест 2.1: Загрузка модуля
    echo "2.1 Загрузка модуля..."
    dmesg -C
    insmod proc_config_module.ko
    sleep 0.5
    
    if [ -f /proc/my_config ]; then
        print_success "Файл /proc/my_config создан"
    else
        print_error "Файл /proc/my_config не создан!"
        return 1
    fi
    
    # Тест 2.2: Чтение дефолтного значения
    echo "2.2 Чтение дефолтного значения..."
    DEFAULT_VALUE=$(cat /proc/my_config)
    echo "Содержимое: $DEFAULT_VALUE"
    
    if [ "$DEFAULT_VALUE" = "default" ]; then
        print_success "Дефолтное значение корректно"
    else
        print_warning "Дефолтное значение: '$DEFAULT_VALUE' (ожидалось 'default')"
    fi
    
    # Тест 2.3: Запись нового значения
    echo "2.3 Запись нового значения 'new value'..."
    echo "new value" > /proc/my_config
    sleep 0.2
    
    NEW_VALUE=$(cat /proc/my_config)
    echo "Новое содержимое: $NEW_VALUE"
    
    if [ "$NEW_VALUE" = "new value" ]; then
        print_success "Значение изменено корректно"
    else
        print_error "Значение не изменилось! Получено: '$NEW_VALUE'"
    fi
    
    # Тест 2.4: Запись длинной строки
    echo "2.4 Запись длинной строки..."
    LONG_STRING=$(python3 -c "print('A' * 300)")
    echo "$LONG_STRING" > /proc/my_config
    sleep 0.2
    
    LONG_VALUE=$(cat /proc/my_config)
    echo "Длина полученной строки: ${#LONG_VALUE}"
    
    if [ ${#LONG_VALUE} -lt 256 ]; then
        print_success "Длинная строка обрезана корректно (< 256 символов)"
    else
        print_warning "Длина строки: ${#LONG_VALUE} (ожидалось < 256)"
    fi
    
    # Тест 2.5: Просмотр логов
    echo "2.5 Логи из dmesg:"
    dmesg | grep proc_config | tail -5
    
    # Выгрузка
    rmmod proc_config_module
    sleep 0.5
    
    if [ ! -f /proc/my_config ]; then
        print_success "Файл /proc/my_config удалён после выгрузки"
    else
        print_error "Файл /proc/my_config остался после выгрузки!"
    fi
    
    print_success "Тест Proc Config Module завершён"
    echo ""
}

# Тест 3: Proc Stats Module (Задание C)
test_proc_stats() {
    print_header "ТЕСТ 3: Proc Stats Module (Задание C)"
    
    # Тест 3.1: Загрузка модуля
    echo "3.1 Загрузка модуля..."
    dmesg -C
    insmod proc_stats_module.ko
    sleep 0.5
    
    if [ -f /proc/sys_stats ]; then
        print_success "Файл /proc/sys_stats создан"
    else
        print_error "Файл /proc/sys_stats не создан!"
        return 1
    fi
    
    # Тест 3.2: Чтение статистики
    echo "3.2 Чтение статистики системы..."
    echo "--- Содержимое /proc/sys_stats ---"
    cat /proc/sys_stats
    echo "----------------------------------"
    
    # Проверка наличия ключевых полей
    if grep -q "Processes:" /proc/sys_stats; then
        print_success "Поле 'Processes' найдено"
    else
        print_warning "Поле 'Processes' не найдено"
    fi
    
    if grep -q "Memory" /proc/sys_stats; then
        print_success "Поля памяти найдены"
    else
        print_warning "Поля памяти не найдены"
    fi
    
    if grep -q "Uptime" /proc/sys_stats; then
        print_success "Поле 'Uptime' найдено"
    else
        print_warning "Поле 'Uptime' не найдено"
    fi
    
    # Тест 3.3: Множественное чтение
    echo "3.3 Повторное чтение (проверка стабильности)..."
    cat /proc/sys_stats > /dev/null
    cat /proc/sys_stats > /dev/null
    print_success "Множественное чтение работает"
    
    # Тест 3.4: Просмотр логов
    echo "3.4 Логи из dmesg:"
    dmesg | grep sys_stats | tail -10
    
    # Выгрузка
    rmmod proc_stats_module
    sleep 0.5
    
    if [ ! -f /proc/sys_stats ]; then
        print_success "Файл /proc/sys_stats удалён после выгрузки"
    else
        print_error "Файл /proc/sys_stats остался после выгрузки!"
    fi
    
    print_success "Тест Proc Stats Module завершён"
    echo ""
}


# Основная функция
main() {
    print_header "АВТОМАТИЧЕСКОЕ ТЕСТИРОВАНИЕ МОДУЛЕЙ ЯДРА"
    echo "Лабораторная работа №5 - Вариант 2"
    echo ""
    
    # Проверка прав root
    if [ "$EUID" -ne 0 ]; then
        print_error "Этот скрипт должен быть запущен с правами root (sudo)"
        exit 1
    fi
    
    # Проверка наличия модулей
    if [ ! -f "hello_module.ko" ] || [ ! -f "proc_config_module.ko" ] || [ ! -f "proc_stats_module.ko" ]; then
        print_error "Модули не собраны! Запустите 'make' сначала."
        exit 1
    fi
    
    cleanup
    
    # Запуск тестов
    test_hello_module || print_error "Тест Hello Module провален"
    test_proc_config || print_error "Тест Proc Config провален"
    test_proc_stats || print_error "Тест Proc Stats провален"
    
    cleanup
    
    print_header "ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ"
    print_success "Проверьте логи выше на наличие ошибок"
    echo ""
    echo "Для финального просмотра dmesg выполните: dmesg | tail -50"
}

# Запуск
main
