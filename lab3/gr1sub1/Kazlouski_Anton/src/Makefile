CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=pstat

all: $(TARGET)

$(TARGET): pstat.c
	$(CC) $(CFLAGS) -o $(TARGET) pstat.c

clean:
	rm -f $(TARGET)
