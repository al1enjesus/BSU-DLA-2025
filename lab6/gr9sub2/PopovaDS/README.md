# Lab6 FUSE - myfuse

Сборка:
  sudo apt update
  sudo apt install -y libfuse3-dev fuse3 pkg-config build-essential
  make

Примеры запуска:
  mkdir -p /tmp/source /tmp/mount
  echo "hello world" > /tmp/source/hello.txt

  # passthrough (A)
  ./myfuse /tmp/source /tmp/mount --mode=passthrough -f

  # rot13 (B, вариант 2)
  ./myfuse /tmp/source /tmp/mount --mode=rot13 -f

  # uppercase (C)
  ./myfuse /tmp/source /tmp/mount --mode=uppercase -f

Размонтирование:
  fusermount -u /tmp/mount
  # если зависло:
  fusermount -uz /tmp/mount

