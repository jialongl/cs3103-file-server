#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "shared_constants.h"
#include "server_constants.h"

int main(int argc, char* argv[]) {
	int listenSoc;		// listening socket
	int connectionSoc;	// connection socket
	pid_t childpid;
	int port;
	struct sockaddr_in serverAddr;
	char receiverBuffer[100];

	printf("Initializing server...\n");

	if (argc == 1) {
		port = DEFAULT_SERVER_PORT;
		printf("Port no. not specified, using default: 6789\n");

	} else if (argc == 2) {
		port = atoi(argv[1]);

	} else {
		printf("Too many arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	printf("Port no: %d\n", port);

	listenSoc = socket(AF_INET, SOCK_STREAM, 0); // `man socket` lah
	if (listenSoc == -1) {
		fprintf(stderr, "Error with socket().\n");
		exit(EXIT_CODE_ERROR);
	}

	// memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSoc,
			 (struct sockaddr *) &serverAddr,
			 sizeof(serverAddr)) == -1) {
		perror("bind()");
		// fprintf(stderr, "Error with bind().\n");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(listenSoc, MAX_NUM_PENDING_CONNECTIONS)) {
		perror("listen()");
		// fprintf(stderr, "Error with listen().\n");
		exit(EXIT_CODE_ERROR);
	}
	printf("Listening...\n");

	// start to run forever for accept-communicate-close cycle.
	while (1) {
		// clear buffers first
		bzero(receiverBuffer, sizeof(receiverBuffer));

		connectionSoc = accept(listenSoc, NULL, NULL);
		if (connectionSoc == -1) {
			perror("accept()");
			// fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}

		else {
			// new TCP connection with a client established.
			// TODO: fork here.

			struct sockaddr_storage clientAddr;
			socklen_t len = sizeof(clientAddr);
			char clientIPString[INET_ADDRSTRLEN];
			int clientPort;

			getpeername(connectionSoc, (struct sockaddr*) &clientAddr, &len);
			if (clientAddr.ss_family == AF_INET) { // IPv4
				struct sockaddr_in *addr = (struct sockaddr_in*) &clientAddr;
				clientPort = addr->sin_port;
				inet_ntop(AF_INET, &addr->sin_addr, clientIPString, sizeof(clientIPString));
			}
			// else if (clientAddr.ss_family == AF_INET6) {

			// 	}

			printf("New client <client_id> connected from: %s#%d\n", clientIPString, clientPort);
		recv:
			if (recv(connectionSoc, receiverBuffer, sizeof(receiverBuffer), 0) != -1) {
				printf("%s", receiverBuffer);
				bzero(receiverBuffer, sizeof(receiverBuffer));
				goto recv;
			}
		}

		// close(connectionSoc);
	}

	return EXIT_CODE_CLEAN;
}

