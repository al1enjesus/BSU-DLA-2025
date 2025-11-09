CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = pstat
SOURCES = pstat.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: clean install