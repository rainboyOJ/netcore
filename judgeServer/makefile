CXX =g++
CXXFLAGS = -std=c++17
INCLUDE = -I../lib -I./judge

server: main.cpp lang_support.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE)

lang_support.o: judge/lang_support.cpp judge/lang_support.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)

