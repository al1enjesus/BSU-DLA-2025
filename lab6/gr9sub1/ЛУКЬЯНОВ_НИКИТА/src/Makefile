CC = gcc
CFLAGS = -Wall -g $(shell pkg-config fuse3 --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs)

all: task_A task_B task_C

myfuse.o: myfuse.c myfuse.h
	$(CC) $(CFLAGS) -c myfuse.c -o myfuse.o

task_A: task_A.c myfuse.o
	$(CC) $(CFLAGS) task_A.c myfuse.o -o task_A $(LDFLAGS)

task_B: task_B.c myfuse.o
	$(CC) $(CFLAGS) task_B.c myfuse.o -o task_B $(LDFLAGS)

task_C: task_C.c myfuse.o
	$(CC) $(CFLAGS) task_C.c myfuse.o -o task_C $(LDFLAGS)

clean:
	rm -f *.o task_A task_B task_C
