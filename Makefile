all: fileserver client

fileserver: server.c
	gcc -g -o fileserver server.c -std=c99 -lpthread -lnsl #-lsocket

client: client.c
	gcc -o client client.c -lpthread -lnsl #-lsocket

clean:
	rm fileserver client
