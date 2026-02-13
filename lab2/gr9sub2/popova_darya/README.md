# Lab2A — Мини-супервизор (popova_darya)

## Кратко
Мини-супервизор на Python, управляющий группой воркеров:
- запускает N воркеров,
- перезапускает упавшие инстансы с ограничением частоты рестартов,
- обрабатывает сигналы для graceful shutdown / reload и переключения профилей нагрузки,
- воркеры имитируют нагрузку и печатают диагностические строки.

## Состав папки
- `supervisor.py` — супервизор (запуск, обработка сигналов, рестарты с лимитом).
- `worker.py` — воркер, имитирующий нагрузку (обрабатывает SIGTERM, SIGUSR1/SIGUSR2).
- `config.json` — конфигурация в формате JSON.
- `run.sh` — демонстрационный скрипт для фонового запуска (с подсказками).
- `REPORT.md` — пример краткого отчёта / шаблон.
- `README.md` — этот файл.

## Требования (VM / WSL / контейнер на базе Debian/Ubuntu)
```bash
sudo apt update
sudo apt install -y python3 python3-pip procps util-linux
# опционально для perf/strace:
sudo apt install -y sysstat strace linux-tools-generic
# Python-зависимость (опционально, для вывода CPU номера):
pip3 install --user psutil
