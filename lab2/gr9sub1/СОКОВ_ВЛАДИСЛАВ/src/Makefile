CC := gcc
CFLAGS := -O2 -Wall -Wextra

all: supervisor cpu_burn

supervisor: supervisor.c
	$(CC) $(CFLAGS) -o supervisor supervisor.c

cpu_burn: cpu_burn.c
	$(CC) $(CFLAGS) -o cpu_burn cpu_burn.c

clean:
	rm -f supervisor cpu_burn