CC = gcc
CFLAGS = -Wall -O2 `pkg-config fuse3 --cflags`
LDFLAGS = `pkg-config fuse3 --libs`
SRCDIR = src
OBJ = $(SRCDIR)/main.o $(SRCDIR)/operations.o $(SRCDIR)/utils.o $(SRCDIR)/transforms.o

all: myfuse

myfuse: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(SRCDIR)/*.o myfuse

