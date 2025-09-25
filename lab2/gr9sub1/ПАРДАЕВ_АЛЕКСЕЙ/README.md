Запуск:
  python3 supervisor.py -c config.json -n 4

Сигналы:
  SIGUSR1 -> воркеры переходят в LIGHT
  SIGUSR2 -> воркеры переходят в HEAVY
  SIGHUP  -> супервизор перечитывает config.json и мягко перезапускает воркеров
  SIGTERM/SIGINT -> graceful shutdown (<=5s)

Проверка nice/affinity:
  ps -o pid,ppid,ni,psr,cmd | grep worker.py
  cat /proc/<pid>/status | grep Cpus_allowed_list
  pidstat -u 1 10
