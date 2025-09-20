CC := gcc
CFLAGS := -O2 -Wall -Wextra -Werror -std=c11

all: cpu_burn mem_touch

cpu_burn: cpu_burn.c
	$(CC) $(CFLAGS) $< -o $@

mem_touch: mem_touch.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f cpu_burn mem_touch

.PHONY: all clean


