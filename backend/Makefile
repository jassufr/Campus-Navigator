CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -Iinclude -Ithird_party
CORE      = src/graph.cpp src/dijkstra.cpp
HDRS      = include/graph.hpp include/dijkstra.hpp

server: src/main.cpp $(CORE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(CORE) -pthread

run: server
	./server ..

test: tests/test_dijkstra
	./tests/test_dijkstra ../data/building.json

tests/test_dijkstra: tests/test_dijkstra.cpp $(CORE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_dijkstra.cpp $(CORE)


audit: tests/audit_routes
	./tests/audit_routes ../data/building.json

tests/audit_routes: tests/audit_routes.cpp $(CORE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ tests/audit_routes.cpp $(CORE)

clean:
	rm -f tests/test_dijkstra tests/audit_routes server

.PHONY: run test clean