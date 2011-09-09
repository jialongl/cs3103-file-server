#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include "shared_constants.h"

int main(int argc, char* argv[]) {
	int connectionSd;
	int serverPort, clientPort;
	char *serverIP;
	struct sockaddr_in serverAddr;
	char senderBuffer[100];
	int bufferLength = sizeof(senderBuffer);

	if (argc == 3) {
		serverPort = atoi(argv[2]);
		serverIP = argv[1];

// 		fprintf(stderr, "Port no. not specified, using default: 6789\n");
// 	} else if (argc == 2) {
// 		port = atoi(argv[1]);

	} else {
		fprintf(stderr, "Wrong arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	fprintf(stderr, "Initializing client...\n");
	fprintf(stderr, "Client ID: iddddddddddd.\n");
	fprintf(stderr, "Server: %s:%d\n", serverIP, serverPort);

	connectionSd = socket(AF_INET, SOCK_STREAM, 0);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(serverIP);
	serverAddr.sin_port = htons(serverPort);

	// connect to server
	if (connect(connectionSd,
				(struct sockaddr*) &serverAddr,
				sizeof(serverAddr)) == -1) {
		fprintf(stderr, "Error with connect().\n");
		exit(EXIT_CODE_ERROR);
	}

	gethostname(senderBuffer, bufferLength);
	struct hostent *host;
	host = gethostbyname(senderBuffer);

	struct in_addr myAddr;
	char *myIP;
	while (*host->h_addr_list) {
		bcopy(*host->h_addr_list++, (char *) &myAddr, sizeof(myAddr));

	}
	myIP = inet_ntoa(myAddr);

	if (send(connectionSd, myIP, strlen(myIP), 0) != strlen(myIP)) {
		fprintf(stderr, "Error: send() sent diff no. of bytes from expected.\n");
		exit(EXIT_CODE_ERROR);
	}

	return EXIT_CODE_CLEAN;
}
