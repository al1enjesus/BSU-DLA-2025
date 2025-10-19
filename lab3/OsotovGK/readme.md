## Как запустить

Запуск `pstat`:

```bash
chmod +x src/pstat.py
python3 src/pstat.py <PID>
```

Запуск имитационного процесса :

```bash
chmod +x src/imitation.py
./src/imitation.py & echo $!
# запомни PID процесса, или используй ps чтобы его найти
```
