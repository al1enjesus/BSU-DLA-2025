CC=gcc
CFLAGS=-O2 -fPIC -Wall -Wextra
LDFLAGS=-ldl -lpthread
OUTDIR=build

all: $(OUTDIR)/libsyscall_spy.so $(OUTDIR)/benchmark $(OUTDIR)/static_test

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/libsyscall_spy.so: src/syscall_spy.c | $(OUTDIR)
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

$(OUTDIR)/benchmark: src/benchmark.c | $(OUTDIR)
	$(CC) -O2 -o $@ $<

$(OUTDIR)/static_test: src/static_test.c | $(OUTDIR)
	$(CC) -static -o $@ $<

clean:
	rm -rf $(OUTDIR) logs/*.log
