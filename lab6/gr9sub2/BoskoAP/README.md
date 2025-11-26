Этот проект представляет собой простую FUSE‑файловую систему, которая читает обычные файлы и применяет к их содержимому преобразования (ROT13, UPPERCASE) на лету.

Структура проекта:

```
project/
├── src/
│   ├── operations.c
│   ├── operations.h
│   ├── transforms.c
│   ├── transforms.h
│   ├── utils.c
│   └── main.c
├── Makefile
└── README.md
```

## 🔧 Требования

* **Linux / macOS** (FUSE зависит от платформы)
* **GCC** или Clang
* **pkg-config**
* **FUSE 3**

### Установка FUSE 3

#### macOS (macFUSE):

1. Скачать и установить macFUSE: [https://osxfuse.github.io/](https://osxfuse.github.io/)
2. Разрешить загрузку расширений в настройках безопасности.

#### Ubuntu / Linux:

```sh
sudo apt install fuse3 libfuse3-dev pkg-config
```

Проверь, что FUSE обнаруживается:

```sh
pkg-config --libs --cflags fuse3
```

## 🛠 Сборка

В корне проекта (там, где Makefile):

```sh
make
```

Появится бинарник:

```
build/virtfs
```

## 🚀 Запуск

Создай директорию для монтирования:

```sh
mkdir mountpoint
```

Запусти файловую систему:

```sh
./build/virtfs mountpoint
```

Теперь можно читать файлы через `mountpoint/`.
Они будут отображаться с применённым преобразованием (например, ROT13 или UPPERCASE — зависит от того, как реализовано в `operations.c`).

## 🧪 Проверка работы

### Просмотр содержимого файла через нашу FS

```sh
cat mountpoint/hello.txt
```

Если исходный `hello.txt` содержит:

```
Hello World!
```

То, например, при ROT13 ты увидишь:

```
Uryyb Jbeyq!
```

## 📌 Демонтаж

После завершения работы:

```sh
fusermount3 -u mountpoint   # Linux
```

или

```sh
umount mountpoint            # macOS
```

## 🧼 Очистка

```sh
make clean
```

## 🛠 Отладка

Можно запустить FUSE в debug‑режиме:

```sh
./build/virtfs -d mountpoint
```
