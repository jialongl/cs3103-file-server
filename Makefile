all: fileserver client

fileserver: server.c
	gcc -g -o fileserver server.c -std=c99
client: client.c
	gcc -g -o client client.c -std=c99

# gcc -g -o fileserver server.c -std=c99 -lpthread -lnsl -lsocket
# gcc -g -o client client.c -std=c99 -lpthread -lnsl -lsocket

# run server
rs:
	./fileserver 8888
# run client
rc:
	./client 7777 < clientCommandsInput

clean:
	rm fileserver client
