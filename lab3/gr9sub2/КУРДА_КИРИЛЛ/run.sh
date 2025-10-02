RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PSTAT_PATH=""

print_header() {
    echo -e "\n${BLUE}===================================================${NC}"
    echo -e "${GREEN}$1${NC}"
    echo -e "${BLUE}===================================================${NC}\n"
}

setup_files() {
    print_header "Подготовка файлов"
    
    if [ -f "src/pstat.py" ]; then
        PSTAT_PATH="src/pstat.py"
        chmod +x "$PSTAT_PATH"
        echo -e "${GREEN}✓${NC} pstat.py найден в src/"
    elif [ -f "pstat.py" ]; then
        PSTAT_PATH="./pstat.py"
        chmod +x "$PSTAT_PATH"
        echo -e "${GREEN}✓${NC} pstat.py найден в текущей директории"
    else
        echo -e "${RED}✗${NC} pstat.py не найден!"
        echo "Проверьте наличие файла в src/ или текущей директории"
        return 1
    fi
    return 0
}

demo_basic() {
    print_header "Демонстрация базового функционала pstat"
    
    echo -e "${YELLOW}1. Анализ процесса init (PID 1)${NC}"
    python3 "$PSTAT_PATH" 1 2>/dev/null | head -20 || echo "Недостаточно прав для чтения"
    
    echo -e "\n${YELLOW}2. Анализ текущего shell (PID $$)${NC}"
    python3 "$PSTAT_PATH" $$
    
    echo -e "\n${YELLOW}3. Создание и анализ тестового процесса${NC}"
    echo "Запускаем sleep 100..."
    sleep 100 &
    TEST_PID=$!
    sleep 0.5
    
    python3 "$PSTAT_PATH" $TEST_PID
    kill $TEST_PID 2>/dev/null || true
    wait $TEST_PID 2>/dev/null || true
}

demo_workload() {
    print_header "Демонстрация с различными нагрузками"
    
    echo -e "${YELLOW}1. Процесс с CPU нагрузкой${NC}"
    cat << 'EOF' > /tmp/cpu_load.py
import time
start = time.time()
while time.time() - start < 3:
    x = sum(i*i for i in range(10000))
EOF
    
    python3 /tmp/cpu_load.py &
    CPU_PID=$!
    sleep 1
    
    echo "Анализ процесса с CPU нагрузкой (PID $CPU_PID):"
    python3 "$PSTAT_PATH" $CPU_PID | grep -E "CPU time|State" || true
    wait $CPU_PID 2>/dev/null || true
    
    echo -e "\n${YELLOW}2. Процесс с памятью${NC}"
    cat << 'EOF' > /tmp/mem_load.py
import os, time
data = bytearray(50 * 1024 * 1024)  # 50 MB
print(f"Allocated 50 MB, PID: {os.getpid()}")
time.sleep(3)
EOF
    
    python3 /tmp/mem_load.py &
    MEM_PID=$!
    sleep 1
    
    echo "Анализ процесса с выделенной памятью (PID $MEM_PID):"
    python3 "$PSTAT_PATH" $MEM_PID | grep -E "VmRSS|RssAnon|RssFile" || true
    wait $MEM_PID 2>/dev/null || true
}

compare_tools() {
    print_header "Сравнение pstat с системными утилитами"
    
    python3 -c "import time; time.sleep(10)" &
    TEST_PID=$!
    sleep 0.5
    
    echo -e "${YELLOW}pstat:${NC}"
    python3 "$PSTAT_PATH" $TEST_PID | grep -E "VmRSS|CPU time|State" || true
    
    echo -e "\n${YELLOW}ps aux:${NC}"
    ps aux | grep -E "PID|^[^ ]*[ ]*$TEST_PID" | grep -v grep || true
    
    if command -v pidstat &> /dev/null; then
        echo -e "\n${YELLOW}pidstat:${NC}"
        pidstat -p $TEST_PID 1 1 || true
    else
        echo -e "\n${YELLOW}pidstat не установлен${NC}"
        echo "Установите: sudo apt install sysstat"
    fi
    
    kill $TEST_PID 2>/dev/null || true
    wait $TEST_PID 2>/dev/null || true
}

advanced_diagnostics() {
    print_header "Расширенная диагностика (strace/perf)"
    
    if ! command -v strace &> /dev/null; then
        echo -e "${YELLOW}strace не установлен${NC}"
        echo "Установите: sudo apt install strace"
        return 0
    fi
    
    echo -e "${YELLOW}Анализ системных вызовов с strace${NC}"
    
    cat << 'EOF' > /tmp/io_test.py
import time, os
for i in range(100):
    with open(f'/tmp/test_{i}.txt', 'w') as f:
        f.write('test' * 100)
    time.sleep(0.01)
    if i % 10 == 0:
        print(f"Processed {i} files")
EOF
    
    python3 /tmp/io_test.py &
    IO_PID=$!
    sleep 0.5
    
    echo "Трассировка процесса с I/O (PID $IO_PID):"
    
    timeout 2 strace -c -p $IO_PID 2>&1 | head -20 || true
    
    # Убиваем процесс и чистим файлы
    kill $IO_PID 2>/dev/null || true
    wait $IO_PID 2>/dev/null || true
    rm -f /tmp/test_*.txt 2>/dev/null || true
    
    # Проверяем perf
    echo -e "\n${YELLOW}Проверка perf:${NC}"
    if command -v perf &> /dev/null; then
        echo -e "${GREEN}perf установлен${NC}"
        echo "Примечание: perf может требовать дополнительных прав в WSL2"
    else
        echo -e "${YELLOW}perf не установлен или недоступен в WSL2${NC}"
        echo "Это нормально для WSL2 окружения"
    fi
    
    return 0
}

check_environment() {
    print_header "Проверка окружения"
    
    if grep -qi microsoft /proc/version; then
        echo -e "${GREEN}✓${NC} Обнаружен WSL2"
    else
        echo -e "${YELLOW}⚠${NC} Не WSL2, работаем в обычном Linux"
    fi
    
    if command -v python3 &> /dev/null; then
        echo -e "${GREEN}✓${NC} Python3 установлен: $(python3 --version)"
    else
        echo -e "${RED}✗${NC} Python3 не найден!"
        echo "Установите: sudo apt install python3"
        return 1
    fi
    
    if [ -d "/proc" ]; then
        echo -e "${GREEN}✓${NC} Файловая система /proc доступна"
    else
        echo -e "${RED}✗${NC} /proc не найдена!"
        return 1
    fi
    
    echo -e "\n${YELLOW}Дополнительные утилиты:${NC}"
    
    for tool in ps top pidstat strace perf; do
        if command -v $tool &> /dev/null; then
            echo -e "  ${GREEN}✓${NC} $tool установлен"
        else
            echo -e "  ${YELLOW}⚠${NC} $tool не установлен (опционально)"
        fi
    done
    
    return 0
}

install_deps() {
    print_header "Установка зависимостей (требует sudo)"
    
    echo "Хотите установить рекомендуемые пакеты? (y/n)"
    read -r response
    
    if [[ "$response" == "y" ]]; then
        sudo apt update
        sudo apt install -y procps sysstat strace || true
        
        if ! command -v perf &> /dev/null; then
            echo -e "${YELLOW}Примечание: perf может быть недоступен в WSL2${NC}"
        fi
    fi
}

run_all_demos() {
    echo -e "${YELLOW}Запуск всех демонстраций...${NC}"
    
    if ! check_environment; then
        echo -e "${RED}Проблемы с окружением!${NC}"
        return 1
    fi
    
    if ! setup_files; then
        echo -e "${RED}Не найден pstat.py!${NC}"
        return 1
    fi
    
    demo_basic
    demo_workload
    compare_tools
    advanced_diagnostics
    
    echo -e "\n${GREEN}Все демонстрации завершены!${NC}"
    return 0
}

main_menu() {
    while true; do
        print_header "Главное меню Lab3"
        
        echo "1) Проверка окружения"
        echo "2) Установка зависимостей"
        echo "3) Базовая демонстрация pstat"
        echo "4) Демонстрация с нагрузками"
        echo "5) Сравнение с системными утилитами"
        echo "6) Расширенная диагностика (strace/perf)"
        echo "7) Запустить все демо"
        echo "q) Выход"
        
        echo -e "\n${YELLOW}Выберите опцию:${NC} "
        read -r choice
        
        case $choice in
            1) check_environment ;;
            2) install_deps ;;
            3) demo_basic ;;
            4) demo_workload ;;
            5) compare_tools ;;
            6) advanced_diagnostics ;;
            7) run_all_demos ;;
            q|Q) 
                echo -e "${GREEN}Выход...${NC}"
                exit 0 
                ;;
            *) 
                echo -e "${RED}Неверный выбор${NC}"
                ;;
        esac
        
        echo -e "\n${YELLOW}Нажмите Enter для продолжения...${NC}"
        read -r
    done
}

main() {
    print_header "Lab3: /proc, метрики и диагностика"
    
    if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
        echo "Использование: $0 [опция]"
        echo "Опции:"
        echo "  --auto    Запустить все демо автоматически"
        echo "  --check   Только проверка окружения"
        echo "  --basic   Только базовая демонстрация"
        echo "  --help    Показать эту справку"
        exit 0
    elif [ "$1" == "--auto" ]; then
        check_environment
        setup_files
        demo_basic
        demo_workload
        compare_tools
        advanced_diagnostics
        exit 0
    elif [ "$1" == "--check" ]; then
        check_environment
        exit 0
    elif [ "$1" == "--basic" ]; then
        setup_files
        demo_basic
        exit 0
    fi
    
    setup_files
    main_menu
}

trap 'echo -e "\n${YELLOW}Прервано пользователем${NC}"; exit 130' INT

main "$@"