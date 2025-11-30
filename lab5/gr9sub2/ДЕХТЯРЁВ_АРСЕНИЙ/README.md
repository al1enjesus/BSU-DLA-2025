# Лабораторная работа №5 — Модули ядра Linux

Автор: **Дехтярёв Арсений, группа 9, подгруппа 2**

## 📌 Содержание проекта

Проект содержит три независимых модуля ядра:

1. **hello_module.ko** — выводит сообщения при загрузке/выгрузке
2. **proc_module.ko** — создаёт `/proc/dehtyarev_info`
3. **char_device.ko** — символьное устройство `/dev/dehtyarev_device`

Структура проекта:

```
lab2/
 ├── Makefile
 └── src/
      ├── hello_module.c
      ├── proc_module.c
      └── char_device.c
```

---

## ✔ Требования

Перед сборкой убедитесь, что установлены:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

---

## 🚧 Сборка проекта

Находясь в `src`, выполните:

```bash
make
```

После успешной сборки модули появятся в каталоге:

```
src/*.ko
```

Очистить сборку:

```bash
make clean
```

---

## 🔌 Установка и удаление модулей

### 1. Установка модуля Hello World

```bash
sudo insmod src/hello_module.ko
dmesg | tail
```

Удаление:

```bash
sudo rmmod hello_module
```

---

### 2. Установка proc-модуля

```bash
sudo insmod src/proc_module.ko
```

Проверка:

```bash
cat /proc/dehtyarev_info
```

Удаление:

```bash
sudo rmmod proc_module
```

---

### 3. Установка символьного устройства

```bash
sudo insmod src/char_device.ko
dmesg | tail
```

В выводе `dmesg` будет сообщение вида:

```
dehtyarev_device registered with major X minor Y
```

Создаём файл устройства:

```bash
sudo mknod /dev/dehtyarev_device c X Y
sudo chmod 666 /dev/dehtyarev_device
```

---

## ✏ Пример работы с символьным устройством

### Запись и чтение:

```bash
echo "Hello" > /dev/dehtyarev_device
cat /dev/dehtyarev_device
```

### Очистка буфера через ioctl:

```c
ioctl(fd, _IO('d', 1));
```

---

## 🧹 Удаление символьного устройства и модуля

```bash
sudo rm /dev/dehtyarev_device
sudo rmmod char_device
```

