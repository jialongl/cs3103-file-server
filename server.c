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
#include "server_constants.h"

int listenSocket;
int connectionSocket;
int serverPort;
int clientPort;
char clientIPString[INET_ADDRSTRLEN];
struct sockaddr_in serverAddr;
struct sockaddr_storage clientAddr;

SharedFileRecord sharedFileRecords[MAX_NUM_SHARED_FILE_RECORDS];
int numberOfRecords = 0;

char senderBuffer[COMMUNICATION_BUFFER_SIZE];
char receiverBuffer[COMMUNICATION_BUFFER_SIZE];

pid_t childpid;

// EFFECTS: closes all sockets this application uses, and exits.
void willApplicationTerminate () {
	printf("Server received SIGINT, exiting...\n");
	close(connectionSocket);
	close(listenSocket);
	exit(EXIT_CODE_CLEAN);
}

void tokenizeString (char *string, char *delim, char tokens[MAX_NUM_TOKENS][MAX_TOKEN_LENGTH], int *numberOfTokens) {
	*numberOfTokens = 0;
	char *thisToken;

	thisToken = strtok(string, delim);
	while (thisToken != NULL && *numberOfTokens < MAX_NUM_TOKENS) {
		printf("token %d=%s", *numberOfTokens, thisToken);
		strncpy(tokens[*numberOfTokens], thisToken, MAX_TOKEN_LENGTH);
		(*numberOfTokens)++;
		thisToken = strtok(NULL, delim);
	}
}

// REQUIRES: receiverBuffer[] containing a COMMAND as null-terminated string.
// EFFECTS: executes the COMMAND logic, populate senderBuffer[] should the COMMAND return any text.
void executeFileserverCommand() {
	// TODO: implement the execution of (different) commands here.
	if ((strncmp(receiverBuffer, COMMAND_HELP, strlen(COMMAND_HELP)) == 0) &&
		(receiverBuffer[strlen(COMMAND_HELP)] == '\n')) {
		strcpy(senderBuffer, "available commands:\n\thelp\t\tdisplay this help message\n\tlist\t\tget a list of available files from the server\n\tshared\t\tprint the list of files shared with other clients\n\treg <file_name>\tregister for sharing <file_name>\n\tget <file_id>\trequest to download <file_id>\n\tquit\t\tquit the application\n");
	}
	else if ((strncmp(receiverBuffer, COMMAND_LIST_AVAILABLE_FILES, strlen(COMMAND_LIST_AVAILABLE_FILES)) == 0) &&
			 (receiverBuffer[strlen(COMMAND_LIST_AVAILABLE_FILES)] == '\n')) {

		char sprintfBuffer[100];
		for (int i=0; i<numberOfRecords; i++) {
			sprintf(sprintfBuffer, "File %d:\t%s\t%s\n",
					sharedFileRecords[i].id,
					sharedFileRecords[i].owner,
					sharedFileRecords[i].filename);
			strncat(senderBuffer, sprintfBuffer, COMMUNICATION_BUFFER_SIZE);
		}
	}
	else if ((strncmp(receiverBuffer, COMMAND_LIST_MY_SHARED_FILES, 5) == 0) &&
			 (receiverBuffer[strlen(COMMAND_LIST_MY_SHARED_FILES)] == '\n')) {

	}
	else if (strncmp(receiverBuffer, COMMAND_REGISTER_FILE, strlen(COMMAND_REGISTER_FILE)) == 0) {
		if (receiverBuffer[strlen(COMMAND_REGISTER_FILE)] != ' ') {
			strcpy(senderBuffer, "\"reg\" requires an argument. Usage: reg <file_name>\n");
		}
		else {
			char tokens[MAX_NUM_TOKENS][MAX_TOKEN_LENGTH];
			int numberOfTokens;

			tokenizeString(receiverBuffer, " ", tokens, &numberOfTokens);

			// deal with tokens[] and sharedFileRecords[]
			for (int i=1; i<numberOfTokens; i++) {
				//TODO: check if the file is already recorded.
				sharedFileRecords[numberOfRecords].id = numberOfRecords;
				strcpy(sharedFileRecords[numberOfRecords].owner, clientIPString);
				strcpy(sharedFileRecords[numberOfRecords].filename, tokens[i]); // tokens[i] contains the filename, and it has a '\n' at the end.
				numberOfRecords++;
			}
		}
	}
	else if ((strncmp(receiverBuffer, COMMAND_DOWNLOAD_FILE, strlen(COMMAND_DOWNLOAD_FILE)) == 0) &&
			 (receiverBuffer[strlen(COMMAND_DOWNLOAD_FILE)] == '\n')) {

	}
	else if ((strncmp(receiverBuffer, COMMAND_QUIT, strlen(COMMAND_QUIT)) == 0) &&
			 (receiverBuffer[strlen(COMMAND_QUIT)] == '\n')) {
		close(connectionSocket);
	}
	else {
		strcpy(senderBuffer, "command not recognized.\n");
	}
}

int main(int argc, char* argv[]) {
	printf("Initializing server...\n");

	if (argc == 1) {
		serverPort = DEFAULT_SERVER_PORT;
		printf("Port no. not specified, using default: 6789\n");

	} else if (argc == 2) {
		serverPort = atoi(argv[1]);

	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_CODE_ERROR);
	}
	signal(SIGINT, willApplicationTerminate); // Close all sockets and exit when application received SIGTERM

	printf("Port no: %d\n", serverPort);

	listenSocket = socket(AF_INET, SOCK_STREAM, 0); // `man socket` lah
	if (listenSocket == -1) {
		fprintf(stderr, "Error with socket().\n");
		exit(EXIT_CODE_ERROR);
	}

	// memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(serverPort);
	if (bind(listenSocket,
			 (struct sockaddr *) &serverAddr,
			 sizeof(serverAddr)) == -1) {
		perror("bind()");
		// fprintf(stderr, "Error with bind().\n");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(listenSocket, MAX_NUM_PENDING_CONNECTIONS)) {
		perror("listen()");
		// fprintf(stderr, "Error with listen().\n");
		exit(EXIT_CODE_ERROR);
	}
	printf("Listening...\n");

	// start to run forever for accept-communicate-close cycle.
	while (1) {
		socklen_t len = sizeof(clientAddr);
		connectionSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);

		if (connectionSocket == -1) {
			perror("accept()");
			// fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}

		else {
			// new TCP connection with a client established.
			// TODO: fork here.

			if (clientAddr.ss_family == AF_INET) { // IPv4
				struct sockaddr_in *addr = (struct sockaddr_in*) &clientAddr;
				clientPort = addr->sin_port;
				inet_ntop(AF_INET, &addr->sin_addr, clientIPString, INET_ADDRSTRLEN);
			}
			// else if (clientAddr.ss_family == AF_INET6) {

			//	}

			printf("New client <client_id> connected from: %s#%d\n", clientIPString, clientPort);

			while(recv(connectionSocket, receiverBuffer, COMMUNICATION_BUFFER_SIZE, 0) != -1) {
				bzero(senderBuffer, COMMUNICATION_BUFFER_SIZE);
				executeFileserverCommand();
				bzero(receiverBuffer, COMMUNICATION_BUFFER_SIZE);

				send(connectionSocket, senderBuffer, COMMUNICATION_BUFFER_SIZE, 0);
			}
			printf("Client <client_id> closed connection.\n");
			close(connectionSocket);
		}
	}

	return EXIT_CODE_CLEAN;
}
