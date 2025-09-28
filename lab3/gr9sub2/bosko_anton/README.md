## Как запустить

Код на Python, сборки не требуется.

Запуск `pstat`:

```bash
chmod +x src/pstat.py
python3 src/pstat.py <PID> --compare
```

Запуск воркера (в отдельном терминале):

```bash
chmod +x src/worker_sync_io.py
./src/worker_sync_io.py & echo $!
# запомни PID процесса, или используй ps чтобы его найти
```
