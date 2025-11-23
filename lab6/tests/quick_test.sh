#!/bin/bash
#
# Быстрый функциональный тест FUSE файловых систем
#

set -e

# Цвета
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== Быстрый функциональный тест ===${NC}"

# Создаём временные директории
TEST_DIR=$(mktemp -d)
MOUNT_POINT=$(mktemp -d)
SOURCE_DIR="$TEST_DIR/source"

echo -e "${BLUE}Создание тестового окружения...${NC}"
mkdir -p "$SOURCE_DIR"
echo "Hello, World!" > "$SOURCE_DIR/test.txt"

cleanup() {
    echo -e "\n${YELLOW}Очистка...${NC}"
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    sleep 1
    rm -rf "$TEST_DIR" "$MOUNT_POINT"
}

trap cleanup EXIT

quick_test() {
    local fs_name="$1"
    echo -e "\n${BLUE}=== Тест $fs_name ===${NC}"
    
    # Запускаем в фоне
    timeout 10 "bin/$fs_name" "$SOURCE_DIR" "$MOUNT_POINT" &
    FS_PID=$!
    
    # Ждём
    sleep 2
    
    if ! mountpoint -q "$MOUNT_POINT"; then
        echo -e "ОШИБКА: Не удалось подмонтировать $fs_name"
        return 1
    fi
    
    # Проверяем чтение файла
    if [ -f "$MOUNT_POINT/test.txt" ]; then
        CONTENT=$(cat "$MOUNT_POINT/test.txt")
        echo "Содержимое файла: '$CONTENT'"
        echo -e "${GREEN}✓ $fs_name работает${NC}"
    else
        echo "ОШИБКА: Файл не найден"
        return 1
    fi
    
    # Отключаем
    fusermount -u "$MOUNT_POINT"
    wait $FS_PID 2>/dev/null || true
    
    return 0
}

# Тестируем все FS
for fs in passthrough_fs rot13_fs uppercase_fs; do
    if quick_test "$fs"; then
        echo -e "${GREEN}✓ $fs прошёл тест${NC}"
    else
        echo -e "✗ $fs провалил тест"
        exit 1
    fi
done

echo -e "\n${GREEN}✓ Все файловые системы работают корректно!${NC}"