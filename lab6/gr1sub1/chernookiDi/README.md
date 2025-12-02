Lab 6 — FUSE filesystems

Сборка:

```bash
sudo apt update
sudo apt install -y libfuse3-dev fuse3 pkg-config build-essential
make
```

Запуск:

Пример passthrough (по умолчанию):

```bash
mkdir -p /tmp/source /mnt/fuse
# в одном терминале (foreground):
./myfuse /tmp/source /mnt/fuse -f -d
# в другом терминале проверяем:
echo "Hello FUSE" > /mnt/fuse/test.txt
cat /mnt/fuse/test.txt
ls -la /mnt/fuse/
```

ROT13 режим (вариант B, вариант 2):

```bash
./myfuse /tmp/source /mnt/fuse -m rot13 -f
# пишем через FUSE, на диске хранится ROT13
echo "Hello" > /mnt/fuse/secret.txt
cat /mnt/fuse/secret.txt   # показывает Hello
cat /tmp/source/secret.txt # показывает Uryyb (зашифровано)
```

Uppercase режим (вариант C, вариант 2):

```bash
./myfuse /tmp/source /mnt/fuse -m uppercase -f
# при чтении через FUSE всё будет в верхнем регистре
```

Логи операций идут в stderr с форматом: [TIMESTAMP] OPERATION: path (result)

Скрипты для тестирования/построения графиков находятся в `tools/`.

Дополнительные инструкции для полного воспроизведения бенчмарков и графиков:

1) Сборка и запуск FUSE

```bash
make
mkdir -p /tmp/source /tmp/mnt_fuse
# Запуск в отдельном терминале (passthrough):
./myfuse /tmp/source /tmp/mnt_fuse -f -d
```

2) Автоматизированный бенчмарк и сбор CSV

```bash
# Запускает latency/throughput/iops и сохраняет CSV в tools/results
# По умолчанию tmpfs тесты пропускаются, если не запускать от root
ITER=50 tools/bench_full.sh
```

3) Построение графиков (рекомендуется virtualenv)

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r tools/requirements.txt
python3 tools/plot_results.py tools/results/latency.csv tools/results/throughput.csv tools/results/iops.csv tools/results
```

Графики будут сохранены в `tools/results/` как `latency_summary.png`, `throughput.png`, `iops.png`.
