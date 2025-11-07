FROM debian:bookworm

# Избегаем интерактивных prompts при установке
ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем необходимые пакеты
RUN apt-get update && apt-get install -y \
    build-essential \
    kmod \
    vim \
    nano \
    sudo \
    wget \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Создаём рабочую директорию
WORKDIR /workspace

# Копируем исходники (будут доступны при монтировании volume)
COPY . /workspace/

# Команда по умолчанию - запуск bash
CMD ["/bin/bash"]
