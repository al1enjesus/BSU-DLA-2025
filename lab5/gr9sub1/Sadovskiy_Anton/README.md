# Лабораторная работ 5. README

## Установка Docker (Ubuntu/Debian)

### Установка
```bash
# Удалить старые версии
sudo apt-get remove docker docker-engine docker.io containerd runc

# Установить зависимости
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg lsb-release

# Добавить официальный GPG ключ Docker
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# Добавить репозиторий
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Установить Docker Engine
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-compose-plugin

# Добавить пользователя в группу docker
sudo usermod -aG docker $USER
newgrp docker
```

### Проверка установки

```bash
docker --version
docker compose version
docker run hello-world
```

## Запуск Docker

```bash
# Перейти в директорию с docker-compose.yml
cd docker

# Собрать образ и запустить контейнер
docker compose up -d --build

# Проверить статус контейнера
docker compose ps

# Войти в контейнер
docker compose exec kernel-dev bash
```

Вы окажетесь внутри контейнера в директории /workspace.

---

## Работа внутри Docker

### Сборка исходников

```bash
# Перейти в директорию с исходниками
cd /workspace/src

# Собрать все модули
make
```

Проверить созданные модули:

```bash
ls -lh *.ko
```

Должны быть созданы:
- hello_module.ko
- proc_config_module.ko
- proc_stats_module.ko

### Запуск тестов
Если вы все еще в директории src:
```bash
chmod +x test_modules.sh
./test_modules.sh
```

После этого в консоль будет выведена информация о прохождении автотестов для данной
лабораторной работы.


## Возможные проблемы

### Не находит заголовки linux-headers
Уточните свою версию ядра:
```bash
uname -r
```

Установите соответствующие заголовочные файлы непосредственно в докере:
```bash
sudo apt install linux-headers-$(uname -r)
```

Удалите из docker compose монтирование:
```
    - /lib/modules:/lib/modules:ro
    - /usr/src:/usr/src:ro
    - /usr/lib:/usr/lib:ro
```

