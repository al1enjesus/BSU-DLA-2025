# Пользовательская файловая система с использованием FUSE

**Автор:** Соков Владислав  
**Назначение:** Примеры простых FUSE файловых систем:
- `passthrough` — зеркалирование директории,
- `rot13` — хранение файлов в ROT13 (шифрование/дешифрование при записи/чтении),
- `upper` — преобразование содержимого в верхний регистр при чтении.

## Структура проекта
```
lab6/
└── gr9sub1/
└── СОКОВ_ВЛАДИСЛАВ/
├── bench/
│   ├── run_bench.sh
│   ├── bench_utils.sh
│   └── ascii_graph.py
├── src/
│   ├── myfuse.c
│   ├── operations.c
│   ├── operations.h
│   └── utils.c
├── Makefile
├── test_fuse.sh
├── README.md
└── REPORT.md
```

## Требования
- Linux с установленными `libfuse3`.
- Компилятор `gcc`, `make`.

Установить на Debian/Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential libfuse3-dev
```

## Сборка

```bash
cd lab6/gr9sub1/СОКОВ_ВЛАДИСЛАВ
make
```

Будет создан исполняемый файл `myfuse`.

## Запуск и примеры

### Passthrough (по умолчанию)

```bash
mkdir -p /tmp/source /mnt/fuse
./myfuse /tmp/source /mnt/fuse -f
```

### ROT13

```bash
./myfuse /tmp/source /mnt/fuse -f --mode=rot13
```

### Uppercase

```bash
./myfuse /tmp/source /mnt/fuse -f --mode=upper
```

> `-f` — форграунд (не уходить в фон), удобно для отладки.

Для размонтирования:

```bash
fusermount -u /mnt/fuse
```

## Тестирование (быстрые проверки)

```bash
# Создание файла через FUSE
echo "Hello FUSE" > /mnt/fuse/test.txt

# Чтение
cat /mnt/fuse/test.txt

# Список
ls -la /mnt/fuse/

# Удаление
rm /mnt/fuse/test.txt
```

Для автоматического теста можно использовать `test_fuse.sh`:

```bash
chmod +x test_fuse.sh
./test_fuse.sh
```
