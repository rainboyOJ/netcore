CXX =g++
CXXFLAGS = -g -std=c++17 -DDEBUG
INCLUDE = -I../lib -I../core -lpthread -lmysqlclient
TARGET_SRC= $(wildcard ../core/*.cpp)
TARGET_OBJ= $(patsubst %.cpp,%.o,$(TARGET_SRC))

LIB_SRC=$(wildcard ../lib/*.cpp)
LIB_OBJECT=$(patsubst %.cpp,%.o,$(LIB_SRC))

server: main.o $(LIB_OBJECT) $(TARGET_OBJ) 
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)

%.o : %.cpp %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)



clean:
	rm -rf server $(TARGET_OBJ) ${LIB_OBJECT}


