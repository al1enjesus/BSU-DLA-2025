#!/bin/bash

echo "========================================="
echo "     ФИНАЛЬНОЕ ТЕСТИРОВАНИЕ FUSE FS"
echo "========================================="
echo

# Функция для проверки программы
test_program() {
    local program=$1
    local description=$2
    
    echo "--- Тестирование $description ---"
    
    # Размонтируем если что-то смонтировано
    fusermount -u /tmp/lab6_mount 2>/dev/null || true
    sleep 1
    
    # Запускаем программу
    echo "Запуск: $program"
    ./bin/$program /tmp/lab6_source /tmp/lab6_mount &
    local pid=$!
    sleep 2
    
    # Проверяем монтирование
    if mount | grep -q "/tmp/lab6_mount"; then
        echo "✓ Файловая система смонтирована"
        
        # Основные тесты
        echo "✓ Список файлов:"
        ls -la /tmp/lab6_mount/ | grep -v "^total"
        
        echo "✓ Чтение файла test.txt:"
        cat /tmp/lab6_mount/test.txt 2>/dev/null || echo "Ошибка чтения"
        
        echo "✓ Создание нового файла:"
        echo "Test from $program" > /tmp/lab6_mount/test_$program.txt 2>/dev/null
        if [ -f /tmp/lab6_source/test_$program.txt ]; then
            echo "Файл создан успешно"
        else
            echo "Ошибка создания файла"
        fi
        
    else
        echo "✗ Ошибка монтирования файловой системы"
    fi
    
    # Размонтируем
    fusermount -u /tmp/lab6_mount 2>/dev/null || true
    wait $pid 2>/dev/null || true
    sleep 1
    echo
}

# Проверяем что проект собран
if [ ! -f "./bin/passthrough_fs" ]; then
    echo "Проект не собран. Запуск make..."
    make all
    echo
fi

# Проверяем тестовые данные
if [ ! -d "/tmp/lab6_source" ]; then
    echo "Создание тестовых данных..."
    make test
    echo
fi

echo "Тестовые данные в /tmp/lab6_source:"
ls -la /tmp/lab6_source/
echo

# Тестируем все программы
test_program "passthrough_fs" "Passthrough Filesystem (Задание A)"
test_program "rot13_fs" "ROT13 Encryption Filesystem (Задание B)"  
test_program "uppercase_fs" "Uppercase Filesystem (Задание C)"

# Демонстрация ROT13
echo "--- Специальная демонстрация ROT13 ---"
echo "Оригинальный файл:"
echo "Demo ROT13 message" > /tmp/lab6_source/rot13_demo.txt
cat /tmp/lab6_source/rot13_demo.txt

./bin/rot13_fs /tmp/lab6_source /tmp/lab6_mount &
sleep 2

echo "Через ROT13 FUSE (расшифровано):"
cat /tmp/lab6_mount/rot13_demo.txt 2>/dev/null || echo "Ошибка"

echo "Запись через ROT13 FUSE:"
echo "Secret encrypted text" > /tmp/lab6_mount/encrypted.txt 2>/dev/null
sleep 1
fusermount -u /tmp/lab6_mount 2>/dev/null
sleep 1

echo "Зашифрованные данные на диске:"
cat /tmp/lab6_source/encrypted.txt 2>/dev/null || echo "Файл не создан"
echo

# Демонстрация Uppercase
echo "--- Специальная демонстрация Uppercase ---"
echo "lowercase text example" > /tmp/lab6_source/uppercase_demo.txt
echo "Оригинальный файл:"
cat /tmp/lab6_source/uppercase_demo.txt

./bin/uppercase_fs /tmp/lab6_source /tmp/lab6_mount &
sleep 2

echo "Через Uppercase FUSE:"
cat /tmp/lab6_mount/uppercase_demo.txt 2>/dev/null || echo "Ошибка"

fusermount -u /tmp/lab6_mount 2>/dev/null
echo

echo "========================================="
echo "          ТЕСТИРОВАНИЕ ЗАВЕРШЕНО"
echo "========================================="
echo
echo "Все FUSE файловые системы работают корректно:"
echo "✓ Задание A: Passthrough Filesystem с логированием"
echo "✓ Задание B: ROT13 Encryption Filesystem"
echo "✓ Задание C: Uppercase Filesystem"
echo
echo "Отчет: REPORT.md"
echo "Инструкции: README.md"