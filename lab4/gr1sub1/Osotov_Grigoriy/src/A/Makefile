CC = gcc
CFLAGS = -fPIC -Wall -O2
LDFLAGS = -shared -ldl
TARGET = libsyscall_spy.so
SRC = syscall_spy.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
