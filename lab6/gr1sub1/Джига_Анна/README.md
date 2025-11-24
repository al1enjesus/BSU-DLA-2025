# Лабораторная работа 6: FUSE Filesystems (Вариант 6)

## 1\. Структура проекта

Файлы исходного кода и сборки организованы внутри директории `src/`.

| Файл / Директория | Назначение |
| :--- | :--- |
| `src/` | Корневая директория исходного кода и сборки. |
| `src/Makefile` | Скрипт сборки. |
| `src/passthrough_fuse.c` | Исходный код для **Задания B (ROT13) (Также выполнено условие задания А)**. |
| `src/passthrough_uppercase.c` | Исходный код для **Задания C (Uppercase) (Также выполнено условие задания А)**. |
| `REPORT.md` | Отчет с анализом и бенчмарками. |
| `images/` | Графики производительности. |

## 2\. Сборка

Поскольку `Makefile` ожидает исходный файл `passthrough_fuse.c`, для сборки всех версий необходимо выполнять команды **внутри папки `src/`**.

### 2.1. Пошаговая компиляция (внутри `src/`)

**В Терминале:**

```bash
# 1. Перейти в директорию с исходными файлами и Makefile
cd src/

# 2. Компиляция Задания B (ROT13)
# Создаст исполняемый файл 'passthrough_fuse'
make

# 3. Переименовать ROT13, чтобы скомпилировать следующую версию
mv passthrough_fuse rot13_fs

# 4. Временно переименовать исходники для компиляции Uppercase
mv passthrough_fuse.c temp.c 
mv passthrough_uppercase.c passthrough_fuse.c 

# 5. Компиляция Задания C (Uppercase)
# Создаст исполняемый файл 'passthrough_fuse'
make

# 6. Переименовать Uppercase и вернуть исходники
mv passthrough_fuse uppercase_fs
mv passthrough_fuse.c passthrough_uppercase.c
mv temp.c passthrough_fuse.c

# 7. Вернуться в корень проекта
cd ..
```

-----

## 3\. Запуск и Проверка

**Все исполняемые файлы (rot13\_fs, uppercase\_fs) созданы внутри папки `src/`.**

### Общая подготовка

```bash
# Создание директорий для теста (в корневой папке проекта)
mkdir -p .source_rot13 .mount_rot13
mkdir -p .source_upper .mount_upper
```

**Важно:** Всегда убедитесь, что точка монтирования свободна перед запуском. Используйте `fusermount -u <точка_монтирования>`.

-----

### 3.1. Задание B: ROT13 Encryption Filesystem

**Запуск:**

```bash
./src/rot13_fs .source_rot13 .mount_rot13 -f
```

**Проверка (в новом терминале):**

```bash
echo "Hello ROT13" > .mount_rot13/test_rot13.txt
cat .mount_rot13/test_rot13.txt
cat .source_rot13/test_rot13.txt
```

-----

### 3.2. Задание C: Uppercase Filesystem

**Запуск:**

```bash
./src/uppercase_fs .source_upper .mount_upper -f
```

**Проверка (в новом терминале):**

```bash
echo "Hello Uppercase" > .mount_upper/test_upper.txt
cat .mount_upper/test_upper.txt
cat .source_upper/test_upper.txt
```

-----

## 4\. Остановка

Для размонтирования файловой системы:

1.  Нажмите `Ctrl+C` в терминале, где запущен исполняемый файл.
2.  Или используйте команду:

<!-- end list -->

```bash
fusermount -u <точка_монтирования>
```