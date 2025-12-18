CC=gcc
CFLAGS=-Wall -Wextra -O2 `pkg-config --cflags fuse3`
LDFLAGS=`pkg-config --libs fuse3`
SRC=src/myfuse.c
TARGET=myfuse

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

