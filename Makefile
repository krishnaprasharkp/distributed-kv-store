CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -pthread

all: server client benchmark

server: src/server.cpp src/lru_cache.h
	$(CXX) $(CXXFLAGS) -o server src/server.cpp

client: src/client.cpp
	$(CXX) $(CXXFLAGS) -o client src/client.cpp

benchmark: src/benchmark.cpp
	$(CXX) $(CXXFLAGS) -o benchmark src/benchmark.cpp

clean:
	rm -f server client benchmark