CC = gcc
CFLAGS = -Wall -O2 `pkg-config fuse3 --cflags`
LDFLAGS = `pkg-config fuse3 --libs`
SRCDIR = src
OBJ = $(SRCDIR)/main.o $(SRCDIR)/operations.o $(SRCDIR)/utils.o $(SRCDIR)/transforms.o
BINDIR = build
TARGET = $(BINDIR)/myfuse

all: $(TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET): $(BINDIR) $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
