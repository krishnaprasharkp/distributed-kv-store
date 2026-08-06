CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

all: server client

server: src/server.cpp
	$(CXX) $(CXXFLAGS) -o server src/server.cpp

client: src/client.cpp
	$(CXX) $(CXXFLAGS) -o client src/client.cpp

clean:
	rm -f server client
