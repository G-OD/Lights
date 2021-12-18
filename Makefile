CCFLAGS=-Ofast -Wall --std=c++17 -march=native
UNAME := $(shell uname)

ifeq ($(UNAME),Linux)
	CLIENT_PARAMS=-lglut -lGL -lpthread
	SERVER_PARAMS=-lpthread
else
	CLIENT_PARAMS=-framework GLUT -framework OpenGL
	SERVER_PARAMS=
endif

default: client

all:	client	server

client:	client.o
	$(CXX) $(CCFLAGS) -o client build/client.o $(CLIENT_PARAMS)

client.o:	src/client.cpp
	$(CXX) $(CCFLAGS) -c src/client.cpp -o build/client.o

server:	server.o
	$(CXX) $(CCFLAGS) -o server build/server.o $(SERVER_PARAMS)

server.o:	src/server.cpp
	$(CXX) $(CCFLAGS) -DSERVER -c src/server.cpp -o build/server.o

clean:
	rm client server *.o
