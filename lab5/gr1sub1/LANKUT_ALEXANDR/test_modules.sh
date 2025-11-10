RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Функция для проверки окружения
check_environment() {
    echo -e "${YELLOW}[*] Проверка окружения...${NC}"
    
    echo "Версия ядра: $(uname -r)"
    echo "GCC версия: $(gcc --version | head -1)"
    echo "Make версия: $(make --version | head -1)"
    
    if [ -d "/lib/modules/$(uname -r)/build" ]; then
        echo -e "${GREEN}[✓] Kernel headers найдены${NC}"
    else
        echo -e "${RED}[✗] Kernel headers НЕ найдены${NC}"
        echo "Для полного тестирования нужны kernel headers"
        echo "Установите: sudo apt install -y linux-headers-\$(uname -r)"
    fi
    echo ""
}

# Функция для сборки модулей
build_modules() {
    echo -e "${YELLOW}[*] Сборка модулей...${NC}"
    
    make clean
    if make; then
        echo -e "${GREEN}[✓] Модули успешно собраны${NC}"
    else
        echo -e "${RED}[✗] Ошибка при сборке модулей${NC}"
        exit 1
    fi
    echo ""
}

# Функция для загрузки и тестирования hello_module
test_hello_module() {
    echo -e "${YELLOW}[*] Тестирование hello_module...${NC}"
    
    # Базовая загрузка
    echo "  1. Загрузка без параметров..."
    if sudo insmod src/hello_module.ko; then
        echo -e "${GREEN}    [✓] Модуль загружен${NC}"
        echo "    Логи:"
        dmesg | tail -3 | sed 's/^/    /'
    else
        echo -e "${RED}    [✗] Ошибка загрузки${NC}"
        return 1
    fi
    
    # Выгрузка
    echo "  2. Выгрузка модуля..."
    if sudo rmmod hello_module; then
        echo -e "${GREEN}    [✓] Модуль выгружен${NC}"
        echo "    Логи:"
        dmesg | tail -3 | sed 's/^/    /'
    else
        echo -e "${RED}    [✗] Ошибка выгрузки${NC}"
        return 1
    fi
    
    # Загрузка с параметром
    echo "  3. Загрузка с пользовательским сообщением..."
    if sudo insmod src/hello_module.ko message="Custom greeting from test"; then
        echo -e "${GREEN}    [✓] Модуль загружен с параметром${NC}"
        echo "    Логи:"
        dmesg | tail -2 | sed 's/^/    /'
    else
        echo -e "${RED}    [✗] Ошибка загрузки${NC}"
        return 1
    fi
    
    # Выгрузка
    sudo rmmod hello_module
    echo ""
}

# Функция для тестирования proc_module
test_proc_module() {
    echo -e "${YELLOW}[*] Тестирование proc_module...${NC}"
    
    echo "  1. Загрузка модуля..."
    if sudo insmod src/proc_module.ko; then
        echo -e "${GREEN}    [✓] Модуль загружен${NC}"
    else
        echo -e "${RED}    [✗] Ошибка загрузки${NC}"
        return 1
    fi
    
    # Проверка /proc файла
    echo "  2. Проверка /proc/my_config..."
    if [ -f "/proc/my_config" ]; then
        echo -e "${GREEN}    [✓] /proc файл создан${NC}"
        echo "    Содержимое:"
        cat /proc/my_config | sed 's/^/      /'
    else
        echo -e "${RED}    [✗] /proc файл не создан${NC}"
        sudo rmmod proc_module
        return 1
    fi
    
    # Второе чтение (проверка счётчика)
    echo "  3. Второе чтение (проверка счётчика)..."
    echo "    Содержимое:"
    cat /proc/student_info | sed 's/^/      /'
    
    # Третье чтение
    echo "  4. Третье чтение (счётчик должен быть 3)..."
    echo "    Содержимое:"
    cat /proc/student_info | sed 's/^/      /'
    
    echo "  5. Выгрузка модуля..."
    sudo rmmod proc_module
    echo -e "${GREEN}    [✓] Модуль выгружен${NC}"
    echo ""
}

# Функция для тестирования sys_stats_module
test_sys_stats_module() {
    echo -e "${YELLOW}[*] Тестирование sys_stats_module...${NC}"
    
    echo "  1. Загрузка модуля..."
    if sudo insmod src/sys_stats_module.ko; then
        echo -e "${GREEN}    [✓] Модуль загружен${NC}"
    else
        echo -e "${RED}    [✗] Ошибка загрузки${NC}"
        return 1
    fi
    
    # Проверка /proc файла
    echo "  2. Проверка /proc/sys_stats..."
    if [ -f "/proc/sys_stats" ]; then
        echo -e "${GREEN}    [✓] /proc файл создан${NC}"
        echo "    Содержимое:"
        cat /proc/sys_stats | sed 's/^/      /'
    else
        echo -e "${RED}    [✗] /proc файл не создан${NC}"
        sudo rmmod sys_stats_module
        return 1
    fi
    echo ""
    
    # Повторное чтение для проверки обновления данных
    echo "  3. Повторное чтение (проверка обновления)..."
    sleep 1
    cat /proc/sys_stats | sed 's/^/    /'
    echo ""
    
    # Логи
    echo "  4. Логи из dmesg:"
    dmesg | grep "sys_stats_module" | tail -5 | sed 's/^/    /'
    
    # Выгрузка
    echo "  5. Выгрузка модуля..."
    sudo rmmod sys_stats_module
    echo -e "${GREEN}    [✓] Модуль выгружен${NC}"
    echo ""
}

# Информация о модулях
info_modules() {
    echo -e "${YELLOW}[*] Информация о модулях...${NC}"
    echo "  Метаданные hello_module:"
    modinfo src/hello_module.ko 2>/dev/null || echo "    (используйте эту команду в VM)"
    echo ""
    echo "  Загруженные модули (если есть):"
    lsmod | head -5
    echo ""
}

# Главная функция
main() {
    check_environment
    
    if command -v make &> /dev/null; then
        if [ -f "Makefile" ]; then
            build_modules
            
            # Тестирование модулей
            test_hello_module || true
            test_proc_module || true
            test_sys_stats_module || true
            
            info_modules
            
            echo -e "${GREEN}=================================="
            echo "Тестирование завершено!"
            echo "==================================${NC}"
        else
            echo -e "${RED}Makefile не найден${NC}"
            exit 1
        fi
    else
        echo -e "${RED}make не установлен${NC}"
        exit 1
    fi
}

main
