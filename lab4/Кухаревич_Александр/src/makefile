CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -fPIC
LDFLAGS = -ldl
LIBSRC = syscall_spy.cpp
LIBOUT = libsyscall_spy.so
BENCHSRC = benchmark.cpp
BENCHOUT = benchmark

.PHONY: all clean spy bench

all: $(LIBOUT) $(BENCHOUT)

$(LIBOUT): $(LIBSRC)
	$(CXX) $(CXXFLAGS) -shared -o $(LIBOUT) $(LIBSRC) $(LDFLAGS)

$(BENCHOUT): $(BENCHSRC)
	$(CXX) $(CXXFLAGS) -pthread -o $(BENCHOUT) $(BENCHSRC)

spy: $(LIBOUT)
	@echo "Built $(LIBOUT)"

bench: $(BENCHOUT)
	@echo "Built $(BENCHOUT)"

clean:
	rm -f $(LIBOUT) $(BENCHOUT) *.out

