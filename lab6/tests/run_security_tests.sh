#!/bin/bash
#
# Тест безопасности FUSE файловых систем
# Проверяет защиту от path traversal атак и других уязвимостей
#

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Тест безопасности FUSE файловых систем ===${NC}"

# Проверяем что все исполняемые файлы существуют
for fs in passthrough_fs rot13_fs uppercase_fs; do
    if [ ! -x "bin/$fs" ]; then
        echo -e "${RED}ОШИБКА: bin/$fs не найден или не исполняем${NC}"
        exit 1
    fi
done

# Создаём временные директории
TEST_DIR=$(mktemp -d)
MOUNT_POINT=$(mktemp -d)
SOURCE_DIR="$TEST_DIR/source"

echo -e "${BLUE}Создание тестового окружения...${NC}"
mkdir -p "$SOURCE_DIR"
echo "safe content" > "$SOURCE_DIR/safe_file.txt"
echo "sensitive data" > "$TEST_DIR/sensitive.txt"

echo -e "${BLUE}Источник: $SOURCE_DIR${NC}"
echo -e "${BLUE}Монтирование: $MOUNT_POINT${NC}"

cleanup() {
    echo -e "\n${YELLOW}Очистка...${NC}"
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    sleep 1
    rm -rf "$TEST_DIR" "$MOUNT_POINT"
}

trap cleanup EXIT

test_path_traversal() {
    local fs_name="$1"
    echo -e "\n${BLUE}=== Тест path traversal для $fs_name ===${NC}"
    
    # Запускаем файловую систему в фоне
    timeout 10 "bin/$fs_name" "$SOURCE_DIR" "$MOUNT_POINT" &
    FS_PID=$!
    
    # Ждём подключения
    sleep 2
    
    # Проверяем что FS подмонтирована
    if ! mountpoint -q "$MOUNT_POINT"; then
        echo -e "${RED}ОШИБКА: Не удалось подмонтировать $fs_name${NC}"
        kill $FS_PID 2>/dev/null || true
        return 1
    fi
    
    # Пытаемся получить доступ к файлу вне source_dir через path traversal
    echo -e "${YELLOW}Попытка path traversal: ../sensitive.txt${NC}"
    
    if ls "$MOUNT_POINT/../sensitive.txt" 2>/dev/null; then
        echo -e "${RED}ОПАСНОСТЬ: Path traversal успешен! Файловая система уязвима!${NC}"
        return 1
    else
        echo -e "${GREEN}OK: Path traversal заблокирован${NC}"
    fi
    
    # Проверяем доступ к нормальным файлам
    if [ -f "$MOUNT_POINT/safe_file.txt" ]; then
        echo -e "${GREEN}OK: Нормальный доступ работает${NC}"
    else
        echo -e "${RED}ОШИБКА: Нет доступа к легальным файлам${NC}"
        return 1
    fi
    
    # Отключаем FS
    fusermount -u "$MOUNT_POINT"
    wait $FS_PID 2>/dev/null || true
    
    return 0
}

# Тестируем все файловые системы
FAILED_TESTS=0

for fs in passthrough_fs rot13_fs uppercase_fs; do
    if test_path_traversal "$fs"; then
        echo -e "${GREEN}✓ $fs прошёл тест безопасности${NC}"
    else
        echo -e "${RED}✗ $fs провалил тест безопасности${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
done

echo -e "\n${BLUE}=== Результат тестирования ===${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}✓ Все тесты безопасности пройдены!${NC}"
    echo -e "${GREEN}Файловые системы защищены от path traversal атак${NC}"
    exit 0
else
    echo -e "${RED}✗ Найдены уязвимости безопасности в $FAILED_TESTS файловых системах${NC}"
    echo -e "${RED}Необходимо дополнительное исследование и исправление${NC}"
    exit 1
fi