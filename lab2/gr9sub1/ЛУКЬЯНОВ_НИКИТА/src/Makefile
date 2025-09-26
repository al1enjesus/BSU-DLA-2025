CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread -lrt

all: supervisor worker

supervisor: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o supervisor

worker: Worker.cpp
	$(CXX) $(CXXFLAGS) Worker.cpp -o worker

clean:
	rm -f supervisor worker
