#!/bin/bash

# Скрипт для демонстрации работы утилиты pstat
# Показывает различные сценарии использования и сравнения с системными утилитами

set -e

echo "=== Демонстрация утилиты pstat ==="
echo

# Проверяем, существует ли pstat и доступен ли для выполнения
if [ ! -x "./src/pstat" ]; then
    echo "Ошибка: pstat не найден или недоступен для выполнения"
    echo "Пожалуйста, скомпилируйте pstat сначала: make"
    exit 1
fi

# Получаем PID текущей оболочки
SHELL_PID=$(ps -o ppid= $$)
echo "PID текущей оболочки: $SHELL_PID"
echo

# Тест 1: Показываем pstat для текущей оболочки
echo "=== Тест 1: pstat для текущей оболочки (PID: $SHELL_PID) ==="
./src/pstat $SHELL_PID
echo

# Тест 2: Показываем pstat для процесса init (PID 1)
echo "=== Тест 2: pstat для процесса init (PID 1) ==="
if [ -d "/proc/1" ]; then
    ./src/pstat 1
else
    echo "Нет доступа к /proc/1 - пропускаем тест с init"
fi
echo

# Тест 3: Сравнение с командой ps
echo "=== Тест 3: Сравнение с командой ps ==="
echo "Вывод ps для PID $SHELL_PID:"
ps -p $SHELL_PID -F
echo

# Тест 4: Сравнение с pidstat, если доступен
echo "=== Тест 4: Сравнение с pidstat ==="
if command -v pidstat &> /dev/null; then
    echo "Вывод pidstat для PID $SHELL_PID:"
    pidstat -p $SHELL_PID 1 1
else
    echo "pidstat недоступен - пропускаем"
fi
echo

# Тест 5: Тесты обработки ошибок
echo "=== Тест 5: Тесты обработки ошибок ==="

# Тест с несуществующим PID
echo "Тест с несуществующим PID (999999):"
./src/pstat 999999 || echo "Ожидаемая ошибка обработана корректно"
echo

# Тест с некорректным PID
echo "Тест с некорректным PID (abc):"
./src/pstat abc || echo "Ожидаемая ошибка обработана корректно"
echo

# Тест с очень большим PID
echo "Тест с очень большим PID (999999999):"
./src/pstat 999999999 || echo "Ожидаемая ошибка обработана корректно"
echo

echo "=== Демонстрация завершена ==="