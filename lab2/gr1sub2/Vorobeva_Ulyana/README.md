## Воспроизводимость

### 1. Сборка бинарников
```bash
./run.sh



Отправить сигналы для переключения режимов:

kill -USR1 <SUP_PID>  # LIGHT
kill -USR2 <SUP_PID>  # HEAVY


Graceful shutdown:

kill -TERM <SUP_PID>


Проверить состояние процессов:

ps -o pid,comm,nice,psr -C supervisor,worker