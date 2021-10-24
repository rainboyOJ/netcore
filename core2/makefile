CXX =g++
CXXFLAGS = -DDEBUG -g -std=c++17 -DDEBUG
INCLUDE = -I../lib -lpthread -lmysqlclient
#TARGET_SRC= $(wildcard ../core/*.cpp)
#TARGET_OBJ= $(patsubst %.cpp,%.o,$(TARGET_SRC))

LIB_SRC=$(wildcard ../lib/*.cpp)
LIB_OBJECT=$(patsubst %.cpp,%.o,$(LIB_SRC))

server: main.cpp connection.cpp sqlconnpool.o $(LIB_OBJECT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE)

#main.o: main.cpp
	#$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)

%.o : %.cpp %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)



clean:
	rm -rf server ${LIB_OBJECT}


