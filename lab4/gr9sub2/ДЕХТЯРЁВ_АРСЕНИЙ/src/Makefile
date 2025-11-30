CXX = g++
CXXFLAGS = -Wall -Wextra
LIBS = -ldl

all: liblogger.so benchmark

liblogger.so: logger.cpp
	$(CXX) -shared -fPIC $(CXXFLAGS) logger.cpp $(LIBS) -o $@

benchmark: benchmark.cpp
	$(CXX) -O2 $(CXXFLAGS) $< -o $@

clean:
	rm -f liblogger.so benchmark

.PHONY: all clean