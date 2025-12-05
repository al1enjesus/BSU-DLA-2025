# Lab2 — Supervisor + Workers

Сборка:
make

Запуск демонстрации:
bash run.sh

Ручной запуск:

./supervisor &
посмотреть воркеров

ps -ef | grep worker
отправить SIGUSR1 всем воркерам (через супервизор)

kill -USR1 <supervisor_pid>
graceful reload

kill -HUP <supervisor_pid>
graceful shutdown

kill -TERM <supervisor_pid>
