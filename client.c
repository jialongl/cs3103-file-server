#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>

#include "shared_constants.h"

int connectionSocket;
int serverPort, clientPort;
char *serverIP;
struct sockaddr_in serverAddr;
char senderBuffer[COMMUNICATION_BUFFER_SIZE];
char serverResponseBuffer[20];

void willApplicationTerminate() {
	printf("Client received SIGINT, exiting...\n");
	close(connectionSocket);
	exit(EXIT_CODE_CLEAN);
}

int main(int argc, char* argv[]) {
	if (argc == 3) {
		serverPort = atoi(argv[2]);
		serverIP = argv[1];

// 		fprintf(stderr, "Port no. not specified, using default: 6789\n");
// 	} else if (argc == 2) {
// 		port = atoi(argv[1]);

	} else {
		fprintf(stderr, "Wrong number of arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	printf("Initializing client...\n");
	printf("Client ID: iddddddddddd.\n");
	printf("Server: %s:%d\n", serverIP, serverPort);

	signal(SIGINT, willApplicationTerminate);
	connectionSocket = socket(AF_INET, SOCK_STREAM, 0);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(serverIP);
	serverAddr.sin_port = htons(serverPort);

	// connect to server
	if (connect(connectionSocket,
				(struct sockaddr*) &serverAddr,
				sizeof(serverAddr)) == -1) {
		fprintf(stderr, "Error with connect().\n");
		exit(EXIT_CODE_ERROR);
	}

	// gethostname(senderBuffer, COMMUNICATION_BUFFER_SIZE);
	// struct hostent *host;
	// host = gethostbyname(senderBuffer);

	// struct in_addr myAddr;
	// char *myIP;
	// while (*host->h_addr_list) {
	// 	bcopy(*host->h_addr_list++, (char *) &myAddr, sizeof(myAddr));
	// }
	// myIP = inet_ntoa(myAddr);

	// if (send(connectionSocket, myIP, strlen(myIP), 0) != strlen(myIP)) {
	// 	fprintf(stderr, "Error: send() sent diff no. of bytes from expected.\n");
	// 	exit(EXIT_CODE_ERROR);
	// }

	// start to type commands forever.
	// clear buffers first
	bzero(senderBuffer, COMMUNICATION_BUFFER_SIZE);
	while (1) {
		fgets(senderBuffer, COMMUNICATION_BUFFER_SIZE, stdin);

		send(connectionSocket, senderBuffer, strlen(senderBuffer), 0); // do not send bytes not inited by user input.
		bzero(senderBuffer, strlen(senderBuffer));
		// int n = recv(connectionSocket, serverResponseBuffer, COMMUNICATION_BUFFER_SIZE, 0);
		// if (n < 0)
		// 	fprintf(stderr, "ERROR reading from socket");
		// fprintf(stderr, "Echo from server: %s", serverResponseBuffer);
	}

	return EXIT_CODE_CLEAN;
}
