#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <unistd.h>

#define EXIT_CODE_ERROR 500 // 500: Internal server error.... :p
#define EXIT_CODE_CLEAN 200 // 200: OK


int main(int argc, char* argv[]) {
	int listenSd;		// listening socket
	int connectionSd;	// connection socket
	pid_t childpid;
	int port;
	struct sockaddr_in serverAddr;

	port = atoi(argv[1]);

	listenSd = socket(AF_INET, SOCK_STREAM, 0); // `man socket` lah
	if (listenSd == -1) {
		fprintf(stderr, "Error with socket().\n");
		exit(EXIT_CODE_ERROR);
	}

	// memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSd,
			 (struct sockaddr *) &serverAddr,
			 sizeof(serverAddr)) == -1) {
		perror("Error with bind().\n");
		// fprintf(stderr, "Error with bind().\n");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(listenSd, 1024)) {
		fprintf(stderr, "Error with listen().\n");
		exit(EXIT_CODE_ERROR);
	}

	while (1) {
		connectionSd = accept(listenSd, NULL, NULL);
		if (connectionSd == -1) {
			fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}
		else
			printf("I received a new connection!");
	}

	return EXIT_CODE_CLEAN;
}
