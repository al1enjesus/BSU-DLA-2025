# Лабораторная работа №6
Кушмар Элина Викторовна, 4 курс 9 группа

## Пользовательские файловые системы на основе FUSE

### Задания: A, B, C (номер в журнале - 11)

---

## Описание

Данный проект содержит три самостоятельные файловые системы, реализованные с использованием библиотеки **FUSE**:

1. **Задание A — Passthrough FS**
   Файловая система полностью зеркалирует содержимое реальной директории.

2. **Задание B — Archive FS (вариант 1)**
   Чтение `.tar` файла как виртуальной read-only файловой системы.

3. **Задание C — Monitoring FS (вариант 1)**
   Passthrough FS, дополненная сбором статистики обращений, доступной в файле `.stats`.

Работа выполнена на macOS (MacBook Pro M1), но также совместима с Linux.

---

# Требования

### macOS:

* **macFUSE** ([https://osxfuse.github.io/](https://osxfuse.github.io/))
* `pkg-config`
* `clang` или `gcc`

### Linux:

* `fuse3` или `fuse`
* `pkg-config`
* `gcc`

---

# Сборка

```
make
```

Целевые сборки:

| Команда              | Назначение          |
| -------------------- | ------------------- |
| `make passthrough`   | Сборка задания A    |
| `make archive_fs`    | Сборка задания B    |
| `make monitoring_fs` | Сборка задания C    |
| `make test`          | Тест passthrough FS |
| `make clean`         | Очистить сборку     |

---

# Структура проекта

```
.
├── passthrough_fuse.c      # Задание A (основа для C)
├── archive_fs.c            # Задание B
├── monitoring_fs.c         # Задание C
├── Makefile
├── README.md
└── REPORT.md               # Полный отчёт
```

---

# Задание A — Passthrough FS

## Запуск

```bash
mkdir -p /tmp/source
mkdir -p /mnt/fuse
./passthrough /tmp/source /mnt/fuse -f
```

## Использование

```bash
echo "hi" > /mnt/fuse/a.txt
cat /mnt/fuse/a.txt
rm /mnt/fuse/a.txt
```

## Завершение

```bash
umount /mnt/fuse
```

---

# Задание B — Archive FS (read-only TAR)

## Запуск

```bash
mkdir -p /mnt/archive
./archive_fs archive.tar /mnt/archive -f
```

## Пример использования

```bash
ls /mnt/archive
cat /mnt/archive/file1.txt
```

## Попытка записи (должна выдавать ошибку)

```
echo 123 > /mnt/archive/file1.txt
# Read-only file system
```

## Завершение

```
umount /mnt/archive
```

---

# Задание C — Monitoring FS (.stats)

## Запуск

```bash
mkdir -p /tmp/mon_src
mkdir -p /mnt/mon
./monitoring_fs /tmp/mon_src /mnt/mon -f
```

## Использование

```bash
echo "data" > /mnt/mon/a.txt
cat /mnt/mon/a.txt
```

Просмотр статистики:

```bash
cat /mnt/mon/.stats
```

Вывод, например:

```
opens: 5
reads: 3
writes: 1
bytes_read: 14
bytes_written: 3
```

## Завершение

```bash
umount /mnt/mon
```

---

# Графики нагрузки

Графики в файлах:

* `latency.png`
* `throughput.png`
* `iops.png`

Эти графики вставлены в отчёт (`REPORT.md`).

---

# Полный отчёт

Полный отчёт находится в файле:

```
REPORT.md
```

---

# Очистка

```bash
make clean
```
