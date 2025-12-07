# FUSE Filesystem — Лабораторная работа 6 (Вариант 9)

## Режимы
- **Passthrough + Monitoring** — зеркало директории с логированием и файлом `.stats`
- **Tar Read-Only** — монтирует `.tar` как read-only ФС

## Сборка
```bash
sudo apt install libfuse3-dev
make