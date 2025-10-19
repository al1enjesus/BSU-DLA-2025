#!/bin/bash

# run.sh - Скрипт для запуска и демонстрации утилиты pstat
# Лабораторная работа 3 - /proc, метрики и диагностика

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Функции для цветного вывода
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Определяем пути
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
PSTAT_SCRIPT="$SRC_DIR/pstat.py"

# Проверка наличия Python
check_python() {
    if ! command -v python3 &> /dev/null; then
        print_error "Python3 не найден. Установите Python3 для работы утилиты."
        exit 1
    fi
    
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    print_info "Используется Python $PYTHON_VERSION"
}

# Проверка прав доступа к /proc
check_proc_access() {
    if [ ! -d "/proc" ]; then
        print_error "Директория /proc не найдена. Запускайте на Linux системе."
        exit 1
    fi
    
    if [ ! -r "/proc/1/stat" ]; then
        print_warning "Ограниченный доступ к /proc. Некоторые данные могут быть недоступны."
    else
        print_success "Доступ к /proc подтвержден"
    fi
}

# Сборка/подготовка утилиты
prepare_pstat() {
    print_info "Подготовка утилиты pstat..."
    
    # Проверяем наличие утилиты
    if [ ! -f "$PSTAT_SCRIPT" ]; then
        print_error "Файл pstat.py не найден по пути: $PSTAT_SCRIPT"
        print_error "Убедитесь, что структура директорий корректна:"
        print_error "САЗОНЧИК_ИВАН/src/pstat.py"
        exit 1
    fi
    
    # Делаем скрипт исполняемым
    chmod +x "$PSTAT_SCRIPT"
    
    print_success "Утилита pstat готова к использованию"
}

# Функция-обертка для запуска pstat
run_pstat() {
    local pid=$1
    python3 "$PSTAT_SCRIPT" "$pid"
}

# Демонстрация работы утилиты
demo_pstat() {
    print_info "Запуск демонстрации утилиты pstat..."
    echo "==========================================="
    
    # Текущий процесс (shell)
    CURRENT_PID=$$
    print_info "1. Анализ текущего shell процесса (PID: $CURRENT_PID):"
    run_pstat $CURRENT_PID
    
    echo
    print_info "2. Сравнение с утилитой ps:"
    ps -p $CURRENT_PID -o pid,ppid,state,utime,stime,rss,nlwp,comm
    
    # Процесс init/systemd (если доступен)
    if [ -r "/proc/1/stat" ]; then
        echo
        print_info "3. Анализ процесса systemd (PID: 1):"
        run_pstat 1
    fi
    
    # Попытка анализа нескольких системных процессов
    echo
    print_info "4. Поиск процессов для анализа..."
    
    # Ищем несколько системных процессов
    PROCESSES=()
    for pid_dir in /proc/[0-9]*/; do
        pid=$(basename "$pid_dir")
        if [ -r "$pid_dir/stat" ] && [ -r "$pid_dir/status" ]; then
            # Проверяем, что это не kernel процесс (ppid != 0)
            ppid=$(awk '{print $4}' "$pid_dir/stat" 2>/dev/null || echo "0")
            if [ "$ppid" -ne 0 ] && [ ${#PROCESSES[@]} -lt 3 ]; then
                PROCESSES+=("$pid")
            fi
        fi
    done
    
    for pid in "${PROCESSES[@]}"; do
        if [ -n "$pid" ] && [ "$pid" -ne $$ ]; then
            echo
            print_info "5. Анализ случайного процесса (PID: $pid):"
            run_pstat "$pid"
            break
        fi
    done
}

# Тестирование конкретного PID
test_specific_pid() {
    local pid=$1
    
    print_info "Тестирование утилиты на процессе $pid..."
    
    if [ ! -d "/proc/$pid" ]; then
        print_error "Процесс $pid не существует или недоступен"
        return 1
    fi
    
    echo "=== pstat вывод ==="
    run_pstat "$pid"
    
    echo -e "\n=== Сравнение с системными утилитами ==="
    
    # ps
    echo "ps:"
    ps -p "$pid" -o pid,ppid,state,utime,stime,rss,nlwp,comm 2>/dev/null || echo "ps: процесс недоступен"
    
    # top (однократный вывод)
    echo -e "\ntop (однократный вывод):"
    top -b -n 1 -p "$pid" 2>/dev/null | grep -E "(PID|$pid)" || echo "top: процесс недоступен"
    
    # pidstat (если установлен)
    if command -v pidstat &> /dev/null; then
        echo -e "\npidstat:"
        pidstat -p "$pid" -r -d 1 1 2>/dev/null || echo "pidstat: данные недоступны"
    fi
}

# Основное меню
show_menu() {
    echo
    print_info "Меню утилиты pstat:"
    echo "1) Демонстрация работы"
    echo "2) Анализ конкретного PID"
    echo "3) Сравнение с системными утилитами"
    echo "4) Выход"
    echo
    read -p "Выберите опцию (1-4): " choice
    
    case $choice in
        1)
            demo_pstat
            ;;
        2)
            read -p "Введите PID для анализа: " pid
            test_specific_pid "$pid"
            ;;
        3)
            test_specific_pid $$
            ;;
        4)
            print_success "Выход"
            exit 0
            ;;
        *)
            print_error "Неверный выбор"
            ;;
    esac
}

# Основная функция
main() {
    echo "==========================================="
    echo "    Утилита pstat - Лабораторная работа 3"
    echo "    Студент: Сазончик Иван"
    echo "==========================================="
    
    # Проверки
    check_python
    check_proc_access
    prepare_pstat
    
    # Обработка аргументов командной строки
    if [ $# -eq 1 ]; then
        # Запуск с PID
        test_specific_pid "$1"
    elif [ $# -eq 0 ]; then
        # Интерактивный режим
        while true; do
            show_menu
            echo
            read -p "Продолжить? (y/n): " continue
            if [[ ! $continue =~ ^[Yy]$ ]]; then
                break
            fi
        done
    else
        print_error "Использование: $0 [PID]"
        echo "  Без аргументов - интерактивный режим"
        echo "  С аргументом PID - анализ конкретного процесса"
        exit 1
    fi
    
    print_success "Выполнение завершено"
}

# Запуск основной функции
main "$@"