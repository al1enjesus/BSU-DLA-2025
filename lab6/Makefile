CC = gcc
CFLAGS = -Wall -Wextra -g `pkg-config --cflags fuse3`
LDLIBS = `pkg-config --libs fuse3`
SRCDIR = src
OBJ = $(SRCDIR)/myfuse.o $(SRCDIR)/operations.o $(SRCDIR)/utils.o

all: myfuse

myfuse: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDLIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) myfuse

.PHONY: all clean