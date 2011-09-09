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
	int listenSd;		// listening socket
	int connectionSd;	// connection socket
	pid_t childpid;
	int port;
	struct sockaddr_in serverAddr;
	char serverBuffer[100];

	fprintf(stderr, "Initializing server...\n");

	if (argc == 1) {
		port = DEFAULT_SERVER_PORT;
		fprintf(stderr, "Port no. not specified, using default: 6789\n");

	} else if (argc == 2) {
		port = atoi(argv[1]);

	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	fprintf(stderr, "Port no: %d\n", port);

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
//		perror("Error with bind(2).\n");
 		fprintf(stderr, "Error with bind().\n");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(listenSd, MAX_NUM_PENDING_CONNECTIONS)) {
		fprintf(stderr, "Error with listen().\n");
		exit(EXIT_CODE_ERROR);
	}
	fprintf(stderr, "Listening...\n");

	// start to run forever for accept-communicate-close cycle.
	while (1) {
		connectionSd = accept(listenSd, NULL, NULL);
		if (connectionSd == -1) {
			fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}

		else if (recv(connectionSd, serverBuffer, sizeof(serverBuffer), 0) != -1) {
			printf("Client <client_id> connecting: %s\n", serverBuffer);
// 			fprintf(stderr, "Received %s connecting from\n", serverBuffer);
		}
	}

	return EXIT_CODE_CLEAN;
}
