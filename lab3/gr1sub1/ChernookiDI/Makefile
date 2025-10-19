CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = pstat
SRCDIR = src
SOURCES = $(SRCDIR)/pstat.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

test: $(TARGET)
	./$(TARGET) $$$$
	@echo "Testing with current shell PID"