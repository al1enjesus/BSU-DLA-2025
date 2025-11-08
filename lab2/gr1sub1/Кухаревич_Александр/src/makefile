CXX=g++
CXXFLAGS=-std=c++17 -O2 -Wall -Wextra

all: supervisor worker

supervisor: supervisor.cpp
	$(CXX) $(CXXFLAGS) supervisor.cpp -o supervisor

worker: worker.cpp
	$(CXX) $(CXXFLAGS) worker.cpp -o worker

clean:
	rm -f supervisor worker
