CC = gcc
CFLAGS = -Wall -O2
PKG = $(shell pkg-config --cflags --libs fuse3)
SRCDIR = src
TARGET = myfuse

all: build

build:
	$(CC) $(CFLAGS) $(SRCDIR)/myfuse.c -o $(TARGET) $(PKG)

clean:
	rm -f $(TARGET)

.PHONY: all build clean