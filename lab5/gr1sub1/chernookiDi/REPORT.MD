# Лабораторная 4 — Системные вызовы

**Студент:** chernookiDi  

## Выбор программ
Вариант 2 (четный)

## Среда выполнения
- ОС: Ubuntu 20.04 (docker)
# Запуск привилегированного контейнера
docker run -it --privileged -v ${PWD}:/workspace ubuntu:22.04 bash -c "
apt update &&
apt install -y kmod procps &&
cd /workspace &&
echo 'Тестируем базовые функции без компиляции' &&
echo '=== Проверка dmesg ===' &&
dmesg | head -5 &&
echo '=== Проверка /proc ===' &&
ls /proc/ | head -10 &&
bash"

# Внутри контейнера
apt update
apt install -y build-essential linux-headers-generic kmod


## Задание A: 


## Задание B: 


## Задание C: 
