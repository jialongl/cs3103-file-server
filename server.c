#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "shared_constants.h"
#include "server_constants.h"

int listenSocket;
int connectionSockets[MAX_NUM_CONNECTED_CLIENTS];
int newConnectionSocket; // used as a temp.

int serverPort;
int clientPorts[MAX_NUM_CONNECTED_CLIENTS];
struct sockaddr_in serverSockaddr;
struct sockaddr_storage clientSockaddrs[MAX_NUM_CONNECTED_CLIENTS];
struct sockaddr_storage newClientSockaddr; // used as a temp.

SharedFileRecord sharedFileRecords[MAX_NUM_SHARED_FILE_RECORDS];
int numberOfRecords = 0;
pthread_mutex_t fileRecordsCounterMutex = PTHREAD_MUTEX_INITIALIZER; // mutex for 'numberOfRecords'

char clientIPStrings[MAX_NUM_CONNECTED_CLIENTS][INET_ADDRSTRLEN];
char clientIDStrings[MAX_NUM_CONNECTED_CLIENTS][MAX_CLIENT_ID_STRLEN];
int numberOfConnectedClients = 0;
pthread_mutex_t connectedClientsCounterMutex = PTHREAD_MUTEX_INITIALIZER; // mutex for 'numberOfConnectedClients'

char senderBuffer[COMMUNICATION_BUFFER_SIZE];
char receiverBuffer[COMMUNICATION_BUFFER_SIZE];
pthread_mutex_t communicationBuffersMutex = PTHREAD_MUTEX_INITIALIZER;

// EFFECTS: closes all sockets this application uses, and exits.
void willApplicationTerminate () {
	printf("Server received SIGINT, exiting...\n");
	for (int i=0; i<MAX_NUM_CONNECTED_CLIENTS; i++) {
		if (connectionSockets[i] != -1)
			close(connectionSockets[i]);
	}
	close(listenSocket);
	exit(EXIT_CODE_CLEAN);
}

void tokenizeString (char *string, char *delim, char tokens[MAX_NUM_COMMAND_TOKENS][MAX_COMMAND_TOKEN_STRLEN], int *numberOfTokens) {
	*numberOfTokens = 0;
	char *thisToken;

	thisToken = strtok(string, delim);
	while (thisToken != NULL && *numberOfTokens < MAX_NUM_COMMAND_TOKENS) {
		strncpy(tokens[*numberOfTokens], thisToken, MAX_COMMAND_TOKEN_STRLEN);
		(*numberOfTokens)++;
		thisToken = strtok(NULL, delim);
	}
}

void displayNewClientConnectedMsg(int clientIndex) {
	struct sockaddr_in *addr = (struct sockaddr_in*) &clientSockaddrs[clientIndex];
	clientPorts[clientIndex] = addr->sin_port;
	inet_ntop(AF_INET, &addr->sin_addr, clientIPStrings[clientIndex], INET_ADDRSTRLEN);

	sprintf(clientIDStrings[clientIndex],
			"client%d",
			clientIndex);
	printf("New client \"%s\" connected from: %s#%d\n",
		   clientIDStrings[clientIndex],
		   clientIPStrings[clientIndex],
		   clientPorts[clientIndex]);
}

// REQUIRES: receiverBuffer[] containing a COMMAND as null-terminated string.
// EFFECTS: executes the COMMAND logic, populate senderBuffer[] should the COMMAND return any text.
void executeFileserverCommand(int clientIndex) {
	if ((strncmp(receiverBuffer, COMMAND_HELP, strlen(COMMAND_HELP)) == 0) &&
		(receiverBuffer[strlen(COMMAND_HELP)] == '\n')) {
		strcpy(senderBuffer, "available commands:\n\thelp\t\tdisplay this help message\n\tlist\t\tget a list of available files from the server\n\tshared\t\tprint the list of files shared with other clients\n\treg <file_name>\tregister for sharing <file_name>\n\tget <file_id>\trequest to download <file_id>\n\tquit\t\tquit the application\n");
	}
	else if ((strncmp(receiverBuffer, COMMAND_LIST_AVAILABLE_FILES, strlen(COMMAND_LIST_AVAILABLE_FILES)) == 0) &&
			 (receiverBuffer[strlen(COMMAND_LIST_AVAILABLE_FILES)] == '\n')) {

		char sprintfBuffer[sizeof(SharedFileRecord)];
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

		char sprintfBuffer[sizeof(SharedFileRecord)];
		for (int i=0; i<numberOfRecords; i++) {
			if (strcmp(sharedFileRecords[i].owner, clientIDStrings[clientIndex]) == 0) {
				sprintf(sprintfBuffer, "File %d:\t%s\t%s\n",
						sharedFileRecords[i].id,
						sharedFileRecords[i].owner,
						sharedFileRecords[i].filename);
				strncat(senderBuffer, sprintfBuffer, COMMUNICATION_BUFFER_SIZE);
			}
		}
	}
	else if (strncmp(receiverBuffer, COMMAND_REGISTER_FILE, strlen(COMMAND_REGISTER_FILE)) == 0) {
		if (receiverBuffer[strlen(COMMAND_REGISTER_FILE)] != ' ') {
			strcpy(senderBuffer, "\"reg\" requires an argument. Usage: reg <file_name>\n");
		}
		else {
			char tokens[MAX_NUM_COMMAND_TOKENS][MAX_COMMAND_TOKEN_STRLEN];
			int numberOfTokens;

			tokenizeString(receiverBuffer, " ", tokens, &numberOfTokens);

			// deal with tokens[] and sharedFileRecords[]
			for (int i=1; i<numberOfTokens; i++) {
				//TODO: check if the file is already recorded.
				sharedFileRecords[numberOfRecords].id = numberOfRecords;
				strcpy(sharedFileRecords[numberOfRecords].owner, clientIDStrings[clientIndex]);
				strcpy(sharedFileRecords[numberOfRecords].filename, tokens[i]); // tokens[i] contains the filename, and it has a '\n' at the end.
				numberOfRecords++;
			}
		}
	}
	else if ((strncmp(receiverBuffer, COMMAND_DOWNLOAD_FILE, strlen(COMMAND_DOWNLOAD_FILE)) == 0) &&
			 (receiverBuffer[strlen(COMMAND_DOWNLOAD_FILE)] == '\n')) {

	}
	else if (((strncmp(receiverBuffer, COMMAND_QUIT, strlen(COMMAND_QUIT)) == 0) && (receiverBuffer[strlen(COMMAND_QUIT)] == '\n')) ||
			 (strlen(receiverBuffer) == 0)) {
		// using "quit" command or Ctrl-D
		// TODO: clean up files registered by that client

		close(connectionSockets[clientIndex]);
		connectionSockets[clientIndex] = -1; // mark as "released"/"re-usable"
	}
	else {
		strcpy(senderBuffer, "command not recognized.\n");
	}
}

int nextClientIndexToAllocate() {
	for (int i=0; i<MAX_NUM_CONNECTED_CLIENTS; i++)
		if (connectionSockets[i] == -1)
			return i;
	return NO_MORE_CLIENTS_SHOULD_CONNECT;
}

void* handleNewConnection(void* clientIndexArg) {
	// ------------- each thread handles one particluar connection. -------------
	int clientIndex = (int) clientIndexArg;

	displayNewClientConnectedMsg(clientIndex);
	send(connectionSockets[clientIndex], clientIDStrings[clientIndex], INET_ADDRSTRLEN, 0);

	// TODO: protect shared memory -- senderBuffer and receiverBuffer
	// pthread_mutex_lock(&communicationBuffersMutex);
	while(recv(connectionSockets[clientIndex], receiverBuffer, COMMUNICATION_BUFFER_SIZE, 0) != -1) {
		bzero(senderBuffer, COMMUNICATION_BUFFER_SIZE);
		executeFileserverCommand(clientIndex);
		bzero(receiverBuffer, COMMUNICATION_BUFFER_SIZE);

		send(connectionSockets[clientIndex], senderBuffer, COMMUNICATION_BUFFER_SIZE, 0);
	}

	// from here, connectionSockets[clientIndex] is closed (by executeFileserverCommand()).
	pthread_mutex_lock(&connectedClientsCounterMutex);
	numberOfConnectedClients--;
	pthread_mutex_unlock(&connectedClientsCounterMutex);

	printf("Client \"%s\" closed connection.\n", clientIDStrings[clientIndex]);
	pthread_exit(NULL); // one thread handles one connectionSocket, so it exits after the socket is closed.
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

	memset(connectionSockets, -1, sizeof(connectionSockets));

	serverSockaddr.sin_family = AF_INET;
	serverSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverSockaddr.sin_port = htons(serverPort);
	if (bind(listenSocket,
			 (struct sockaddr *) &serverSockaddr,
			 sizeof(serverSockaddr)) == -1) {
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
		socklen_t len = sizeof(struct sockaddr_storage);
		newConnectionSocket = accept(listenSocket, (struct sockaddr*) &newClientSockaddr, &len);

		if (newConnectionSocket == -1) {
			perror("accept()");
			// fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}
		else if (newClientSockaddr.ss_family != AF_INET) {
			strcpy(senderBuffer, MSG_ONLY_SUPPORT_IPV4);
			send(newConnectionSocket, senderBuffer, COMMUNICATION_BUFFER_SIZE, 0);
			close(newConnectionSocket);
		}
		else if (numberOfConnectedClients == MAX_NUM_CONNECTED_CLIENTS) {
		// else if (nextClientIndexToAllocate() == NO_MORE_CLIENTS_SHOULD_CONNECT) {
			strcpy(senderBuffer, MSG_MAX_NUM_CLIENTS_REACHED);
			send(newConnectionSocket, senderBuffer, COMMUNICATION_BUFFER_SIZE, 0);
			close(newConnectionSocket);
		}
		else {
			// new TCP connection with a client established.
			int clientIndex = nextClientIndexToAllocate();
			connectionSockets[clientIndex] = newConnectionSocket;
			clientSockaddrs[clientIndex] = newClientSockaddr;

			pthread_mutex_lock(&connectedClientsCounterMutex);
				numberOfConnectedClients++;
			pthread_mutex_unlock(&connectedClientsCounterMutex);

			pthread_t newConnectionThread;
			if (pthread_create(&newConnectionThread, NULL, handleNewConnection, (void*)clientIndex) != 0) {
				perror("pthread_create()");
			}
			// nothing to be done for parent thread -- just get out of the loop and listen for another connection.
		}
	}
	return EXIT_CODE_CLEAN;
}
