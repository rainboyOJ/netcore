CXX =g++
CXXFLAGS = -g -std=c++17
INCLUDE = -I../lib -I./judge -lpthread
target= lang_support.o judgeServer.o judge.o

LIB_SRC=$(wildcard ../lib/*.cpp)
LIB_OBJECT=$(patsubst %.cpp,%.o,$(LIB_SRC))

server: main.cpp $(LIB_OBJECT) $(target) 
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE)

client: client.cpp client.hpp $(LIB_OBJECT) $(target)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE)

lang_support.o: judge/lang_support.cpp judge/lang_support.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)

judge.o: judge/judge.cpp judge/judge.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)


%.o : %.cpp %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)



clean:
	rm -rf server client $(target) ${LIB_OBJECT}

