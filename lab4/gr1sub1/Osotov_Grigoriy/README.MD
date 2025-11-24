# Лабораторная 4
## Задание A (LD_PRELOAD)

### Установка зависимостей:
   sudo apt update && sudo apt install -y build-essential gcc make curl wget

### Запуск сценария:

1. Выберите target:
   - curl
   - wget
   - all (curl + wget)

2. Выберите список адресов для запросов, например google.com, либо оставьте пустым
3. ./demo.sh <target> \[\<adress>, ...]

### Выходные данные:
- Консольный вывод.
- файл(ы) log/A/logs_\<target>_safe.txt, содержащий готовый лог файл, который можно передавать во вне.

### Статический эксперимент
сборка:
```
gcc -static hello_world_f_static.c
```

Запуск:
```
./a.out
```

## Задание B
Для получения статистики в виде таблицы:
```
./demo.sh
```

Для perf (после demo.sh или Make сборки):

```
sudo apt install linux-perf
perf ./benchmark
```