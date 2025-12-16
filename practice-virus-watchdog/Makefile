CC = clang
CFLAGS = -Wall -Wextra -O2 -std=gnu11
LDFLAGS =
TARGET = telegram_watchdog
SOURCES = watchdog.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	sudo ./$(TARGET)

debug: $(TARGET)
	./$(TARGET)

.PHONY: all clean run debug