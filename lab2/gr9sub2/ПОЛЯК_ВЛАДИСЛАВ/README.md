## Запуск супервизора

cd src

python3 supervisor.py config.json

## Тестирование функциональности

**Переключение режимов работы**
# Найти PID супервизора
ps aux | grep supervisor.py

# Переключение в легкий режим (SIGUSR1)
kill -USR1 <PID_супервизора>

# Переключение в тяжелый режим (SIGUSR2)  
kill -USR2 <PID_супервизора>

**Graceful reload (перезагрузка конфигурации)**
kill -HUP <PID_супервизора>

**Graceful shutdown (корректное завершение)**
kill -TERM <PID_супервизора>
# или
kill <PID_супервизора>

**Автоматическая демонстрация**
# Скрипт демонстрирует все функции супервизора
chmod +x run.sh
./run.sh
