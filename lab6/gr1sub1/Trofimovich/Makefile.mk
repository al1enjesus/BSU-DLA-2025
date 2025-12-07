CC = gcc
CFLAGS = -Wall -Wextra -O2 `pkg-config fuse3 --cflags`
LDFLAGS = `pkg-config fuse3 --libs`

SRC = myfuse.c operations.c utils.c
TARGET = myfuse

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
