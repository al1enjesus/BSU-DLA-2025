CC = gcc
CFLAGS = -Wall -g $(shell pkg-config fuse3 --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs)

all: fs_pass fs_archive fs_monitor

fs_pass: task_a_passthrough.c
	$(CC) $(CFLAGS) task_a_passthrough.c -o fs_pass $(LDFLAGS)

fs_archive: task_b_archive.c
	$(CC) $(CFLAGS) task_b_archive.c -o fs_archive $(LDFLAGS)

fs_monitor: task_c_monitor.c
	$(CC) $(CFLAGS) task_c_monitor.c -o fs_monitor $(LDFLAGS)

clean:
	rm -f fs_pass fs_archive fs_monitor

