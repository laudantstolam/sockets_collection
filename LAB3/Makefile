GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

all: server client

server: server.c
	gcc -pthread -o server server.c

client: client.c
	gcc -pthread $(GTK_CFLAGS) -o client client.c $(GTK_LIBS)

clean:
	rm -f server client

run-server: server
	./server

run-client: client
	./client