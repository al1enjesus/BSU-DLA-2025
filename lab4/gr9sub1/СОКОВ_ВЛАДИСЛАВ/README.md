# Лабораторная 4 — Системные вызовы

Путь: `lab4/gr9sub1/СОКОВ_ВЛАДИСЛАВ`

## Как собрать
```bash
sudo apt update && sudo apt install -y build-essential gcc gdb strace ltrace linux-tools-common linux-tools-generic
cd lab4/gr9sub1/СОКОВ_ВЛАДИСЛАВ
make all
```

## Как запустить автоматизированно (всё запустится и логи будут в logs/)
```bash
chmod +x run.sh
./run.sh
```

Если `perf` требует root, `run.sh` попытается вызвать `sudo perf stat ...`. Перед запуском бенчмарка рекомендуется установить governor в performance:
```bash
sudo apt install -y cpufrequtils
sudo cpufreq-set -r -g performance
```

## Какие файлы появятся
- `libsyscall_spy.so` — библиотека для LD_PRELOAD
- `benchmark` — бенчмарк, печатает средние циклы
- `hello_static` — статический бинарник для демонстрации, что LD_PRELOAD не работает
- `open_bench` — бенчмарк с open() для эксперимента с кэшем
- `logs/` — результаты экспериментов (включая perf_stat.out)