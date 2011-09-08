all: server client

server: server.c
	gcc -o fileserver server.c -lpthread -lnsl -lsocket

client: client.c
	gcc -o client client.c -lpthread -lnsl -lsocket
