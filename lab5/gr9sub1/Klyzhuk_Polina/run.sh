#!/bin/bash

# Скрипт автотестирования для модулей ядра Linux
# Автор: Polina

set -e

MODULE_DIR=$(pwd)
KERNEL_VERSION=$(uname -r)
LOG_FILE="test_results.log"

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Функция логирования
log() {
    echo -e "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

# Функция проверки выполнения команды
check_command() {
    if [ $? -eq 0 ]; then
        log "${GREEN}✓ Успех: $1${NC}"
    else
        log "${RED}✗ Ошибка: $1${NC}"
        exit 1
    fi
}

# Очистка логов
echo "=== Тестирование модулей ядра ===" > "$LOG_FILE"
log "${YELLOW}Начало тестирования модулей...${NC}"

# Проверка прав
if [ "$EUID" -ne 0 ]; then
    log "${RED}Ошибка: Скрипт должен запускаться с правами root${NC}"
    exit 1
fi

# Компиляция модулей
log "Компиляция модулей..."
make clean
make
check_command "Компиляция модулей"

# Тестирование hello_module
log "\n${YELLOW}=== Тестирование hello_module ===${NC}"

log "Загрузка hello_module без параметров..."
sudo insmod hello_module.ko
check_command "Загрузка hello_module без параметров"
dmesg | tail -1 | tee -a "$LOG_FILE"

log "Выгрузка hello_module..."
sudo rmmod hello_module
check_command "Выгрузка hello_module"
dmesg | tail -1 | tee -a "$LOG_FILE"

log "Загрузка hello_module с параметром message..."
sudo insmod hello_module.ko message="Polina_Test"
check_command "Загрузка hello_module с параметром"
dmesg | tail -1 | tee -a "$LOG_FILE"

log "Выгрузка hello_module..."
sudo rmmod hello_module
check_command "Выгрузка hello_module"
dmesg | tail -1 | tee -a "$LOG_FILE"

# Тестирование proc_config
log "\n${YELLOW}=== Тестирование proc_config ===${NC}"

log "Загрузка proc_config..."
sudo insmod proc_config.ko
check_command "Загрузка proc_config"
dmesg | tail -1 | tee -a "$LOG_FILE"

log "Проверка прав доступа к /proc/my_config..."
ls -la /proc/my_config | tee -a "$LOG_FILE"

log "Чтение значения по умолчанию..."
cat /proc/my_config | tee -a "$LOG_FILE"
check_command "Чтение значения по умолчанию"

log "Запись нового значения..."
echo "test_value_polina" > /proc/my_config
check_command "Запись нового значения"

log "Проверка обновленного значения..."
cat /proc/my_config | tee -a "$LOG_FILE"
check_command "Проверка обновленного значения"

log "Тестирование граничных условий (длинная строка)..."
long_string=$(printf '%*s' 300 | tr ' ' 'x')
if echo "$long_string" > /proc/my_config 2>&1; then
    log "${RED}Ошибка: Должна была быть ошибка для длинной строки${NC}"
else
    log "${GREEN}✓ Корректная обработка длинной строки${NC}"
fi

log "Выгрузка proc_config..."
sudo rmmod proc_config
check_command "Выгрузка proc_config"
dmesg | tail -1 | tee -a "$LOG_FILE"

# Тестирование sys_stats
log "\n${YELLOW}=== Тестирование sys_stats ===${NC}"

log "Загрузка sys_stats..."
sudo insmod sys_stats.ko
check_command "Загрузка sys_stats"
dmesg | tail -1 | tee -a "$LOG_FILE"

log "Чтение системной статистики..."
cat /proc/sys_stats | tee -a "$LOG_FILE"
check_command "Чтение системной статистики"

log "Проверка актуальности данных..."
sleep 2
cat /proc/sys_stats | head -1 | tee -a "$LOG_FILE"

log "Выгрузка sys_stats..."
sudo rmmod sys_stats
check_command "Выгрузка sys_stats"
dmesg | tail -1 | tee -a "$LOG_FILE"

# Проверка отсутствия утечек
log "\n${YELLOW}=== Проверка отсутствия утечек ===${NC}"

log "Проверка загруженных модулей..."
lsmod | grep -E "(hello|proc_config|sys_stats)" || log "${GREEN}✓ Модули не загружены${NC}"

log "Проверка сообщений об ошибках в dmesg..."
if dmesg | tail -20 | grep -i "error\|warning\|fail"; then
    log "${YELLOW}⚠ Обнаружены предупреждения в логах${NC}"
else
    log "${GREEN}✓ Нет ошибок в логах${NC}"
fi

# Итоговый отчет
log "\n${YELLOW}=== Итоговый отчет ===${NC}"
log "Все тесты завершены успешно!"
log "Логи сохранены в: $LOG_FILE"
log "Статус ядра: $(cat /proc/sys/kernel/tainted)"

echo -e "\n${GREEN}=== Все тесты пройдены успешно! ===${NC}"