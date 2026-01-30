# Мини-супервизор с воркерами

## Файлы
- `supervisor.sh` - основной супервизор
- `run.sh` - демонстрационные сценарии  
- `supervisor.env` - конфигурация (создается автоматически)

## Быстрый старт

```bash
# Сделать скрипты исполняемыми
chmod +x supervisor.sh run.sh

# Запустить все демонсценарии
./run.sh all

# Или запустить супервизор напрямую
./supervisor.sh
```

## Управление супервизором

### Запуск супервизора
```bash
./supervisor.sh
```

### Сигналы управления
```bash
# Найти PID супервизора
SUPER_PID=$(pgrep -f "supervisor.sh" | head -1)

# Graceful shutdown
kill -TERM $SUPER_PID

# Reload конфигурации
kill -HUP $SUPER_PID

# Переключить в легкий режим
kill -USR1 $SUPER_PID

# Переключить в тяжелый режим  
kill -USR2 $SUPER_PID
```

## Демонстрационные сценарии

### Все демо сразу
```bash
./run.sh all
```

### Отдельные сценарии
```bash
# Graceful shutdown
./run.sh shutdown

# Reload конфигурации
./run.sh reload

# Переключение режимов нагрузки
./run.sh mode

# Тестирование политики рестартов
./run.sh restart
```

## Конфигурация

Файл `supervisor.env` создается автоматически:
```bash
WORKERS=4
MODE_HEAVY_WORK_US=900000
MODE_HEAVY_SLEEP_US=100000
MODE_LIGHT_WORK_US=200000
MODE_LIGHT_SLEEP_US=800000
```

### Изменение конфигурации
```bash
# Увеличить количество воркеров
sed -i 's/WORKERS=3/WORKERS=4/' supervisor.env
kill -HUP $(pgrep -f "supervisor.sh")

# Изменить параметры нагрузки
sed -i 's/MODE_HEAVY_WORK_US=9000/MODE_HEAVY_WORK_US=5000/' supervisor.env
kill -HUP $(pgrep -f "supervisor.sh")
```

## Мониторинг

### Просмотр процессов
```bash
ps aux | grep -E "(supervisor|worker)" | grep -v grep
```

### Просмотр логов
```bash
tail -f supervisor.log
```

### Статистика воркеров
```bash
grep "Worker" supervisor.log | tail -10
```

## Сценарии изменения нагрузки

### Циклическое переключение режимов
```bash
./supervisor.sh &
SUPER_PID=$!

while true; do
    echo "Heavy mode"
    kill -USR2 $SUPER_PID
    sleep 10
    
    echo "Light mode" 
    kill -USR1 $SUPER_PID
    sleep 10
done
```

### Динамическое изменение количества воркеров
```bash
# Увеличить нагрузку
echo "WORKERS=6" > supervisor.env
kill -HUP $(pgrep -f "supervisor.sh")

# Уменьшить нагрузку
echo "WORKERS=2" > supervisor.env  
kill -HUP $(pgrep -f "supervisor.sh")
```

## Завершение работы

```bash
# Graceful shutdown
kill -TERM $(pgrep -f "supervisor.sh")

# Принудительное завершение (если необходимо)
pkill -f "supervisor.sh"
```

## Примечания

- Для отправки сигналов требуется тот же пользователь, что запускал супервизор
- Логи пишутся в `supervisor.log`
- При изменении конфигурации требуется SIGHUP
- Воркеры автоматически перезапускаются при падении (с ограничением частоты)
