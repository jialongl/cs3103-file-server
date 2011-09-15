all: fileserver client

fileserver: server.c
	gcc -Wall -g -o fileserver server.c -std=gnu99
client: client.c
	gcc -Wall -g -o client client.c

# gcc -g -o fileserver server.c -std=c99 -lpthread -lnsl -lsocket
# gcc -g -o client client.c -std=c99 -lpthread -lnsl -lsocket

# run server
rs:
	./fileserver 8888
# run client
rc:
	./client 127.0.0.1 8888

kill:
	killall "./fileserver" "./client"

clean:
	rm fileserver client
