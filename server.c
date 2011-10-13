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

char cmdResultsBuffer[CMD_BUFFER_SIZE];
char cmdRecvingBuffer[CMD_BUFFER_SIZE];
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
	printf("New client \"%s\" connected from: %s#%d\n",
		   clientIDStrings[clientIndex],
		   clientIPStrings[clientIndex],
		   clientPorts[clientIndex]);
}

void trimTrailingSpaces(char* string) {

}

int nextFileRecordIndexToAllocate() {
	for (int i=0; i<MAX_NUM_SHARED_FILE_RECORDS; i++) {
		if (sharedFileRecords[i].valid == FILE_RECORD_VACANT)
			return i;
	}
	return NO_MORE_FILES_SHOULD_BE_ADDED;
}

// REQUIRES: cmdRecvingBuffer[] containing a COMMAND as null-terminated string.
// EFFECTS: executes the COMMAND logic, populate cmdResultsBuffer[] should the COMMAND return any text.
void executeFileserverCommandAsClient(int clientIndex) {
	trimTrailingSpaces(cmdRecvingBuffer);

	if ((strncmp(cmdRecvingBuffer, COMMAND_HELP, strlen(COMMAND_HELP)) == 0) &&
		(cmdRecvingBuffer[strlen(COMMAND_HELP)] == '\n')) {
		strcpy(cmdResultsBuffer, "available commands:\n\thelp\t\tdisplay this help message\n\tlist\t\tget a list of available files from the server\n\tshared\t\tprint the list of files shared with other clients\n\treg <file_name>\tregister for sharing <file_name>\n\tget <file_id>\trequest to download <file_id>\n\tquit\t\tquit the application\n");
	}
	else if ((strncmp(cmdRecvingBuffer, COMMAND_LIST_AVAILABLE_FILES, strlen(COMMAND_LIST_AVAILABLE_FILES)) == 0) &&
			 (cmdRecvingBuffer[strlen(COMMAND_LIST_AVAILABLE_FILES)] == '\n')) {

		char sprintfBuffer[sizeof(SharedFileRecord)];
		for (int i=0; i<numberOfRecords; i++) {
			if (sharedFileRecords[i].valid == FILE_RECORD_VACANT) continue;
			sprintf(sprintfBuffer, "File %d:\t%s\t%s\n",
					i,
					sharedFileRecords[i].owner,
					sharedFileRecords[i].filename);
			strncat(cmdResultsBuffer, sprintfBuffer, CMD_BUFFER_SIZE);
		}
	}
	else if ((strncmp(cmdRecvingBuffer, COMMAND_LIST_MY_SHARED_FILES, 5) == 0) &&
			 (cmdRecvingBuffer[strlen(COMMAND_LIST_MY_SHARED_FILES)] == '\n')) {

		char sprintfBuffer[sizeof(SharedFileRecord)];
		for (int i=0; i<numberOfRecords; i++) {
			if (sharedFileRecords[i].valid == FILE_RECORD_VACANT) continue;
			if (strcmp(sharedFileRecords[i].owner, clientIDStrings[clientIndex]) == 0) {
				sprintf(sprintfBuffer, "F%d:\t%s\t%s\n",
						i,
						sharedFileRecords[i].owner,
						sharedFileRecords[i].filename);
				strncat(cmdResultsBuffer, sprintfBuffer, CMD_BUFFER_SIZE);
			}
		}
	}
	else if (strncmp(cmdRecvingBuffer, COMMAND_REGISTER_FILE, strlen(COMMAND_REGISTER_FILE)) == 0) {
		char tokens[MAX_NUM_COMMAND_TOKENS][MAX_COMMAND_TOKEN_STRLEN];
		int numberOfTokens;
		tokenizeString(cmdRecvingBuffer, " \n", tokens, &numberOfTokens);

		if (numberOfTokens == 1) {
			strcpy(cmdResultsBuffer, "\"reg\" requires an argument. Usage: reg <file_name>\n");
		}
		else {

			// add to sharedFileRecords[]
			for (int i=1; i<numberOfTokens; i++) {
				int fileIndex = nextFileRecordIndexToAllocate();
				if (fileIndex == NO_MORE_FILES_SHOULD_BE_ADDED) {
					strcpy(cmdResultsBuffer, "Sorry, no more files can be registered at this time. Wait till some clients disconnect.");

				} else {
					sharedFileRecords[fileIndex].valid = FILE_RECORD_IN_USE;
					strcpy(sharedFileRecords[fileIndex].owner, clientIDStrings[clientIndex]);
					strcpy(sharedFileRecords[fileIndex].filename, tokens[i]); // tokens[i] (i>=1) contains the filename, and it has a '\n' at the end.
					pthread_mutex_lock(&fileRecordsCounterMutex);
					numberOfRecords++;
					pthread_mutex_unlock(&fileRecordsCounterMutex);
				}
			}
		}
	}
	else if ((strncmp(cmdRecvingBuffer, COMMAND_DOWNLOAD_FILE, strlen(COMMAND_DOWNLOAD_FILE)) == 0) &&
			 (cmdRecvingBuffer[strlen(COMMAND_DOWNLOAD_FILE)] == '\n')) {

	}
	else if (((strncmp(cmdRecvingBuffer, COMMAND_QUIT, strlen(COMMAND_QUIT)) == 0) && (cmdRecvingBuffer[strlen(COMMAND_QUIT)] == '\n')) ||
			 (strlen(cmdRecvingBuffer) == 0)) {
		// using "quit" command or Ctrl-D

		printf("Client \"%s\" quitting...\n", clientIDStrings[clientIndex]);

		char namesOfFilesToBeErased[MAX_NUM_SHARED_FILE_RECORDS * MAX_COMMAND_TOKEN_STRLEN];
		// "clean up" files registered by that client
		for (int i=0; i<numberOfRecords; i++) {
			if (strcmp(sharedFileRecords[i].owner, clientIDStrings[clientIndex]) == 0) {
				sharedFileRecords[i].valid = FILE_RECORD_VACANT;
				strcat(namesOfFilesToBeErased, SharedFileRecord[i].filename);
				strcat(namesOfFilesToBeErased, ", ");
			}
		}
		printf("Erased shared files from server list: %s\n", namesOfFilesToBeErased);

		close(connectionSockets[clientIndex]);
		connectionSockets[clientIndex] = -1; // mark as "released"/"re-usable"
		
		pthread_mutex_lock(&connectedClientsCounterMutex);
		numberOfConnectedClients--;
		pthread_mutex_unlock(&connectedClientsCounterMutex);
	}
	else {
		strcpy(cmdResultsBuffer, "command not recognized.\n");
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

	struct sockaddr_in *addr = (struct sockaddr_in*) &clientSockaddrs[clientIndex];
	clientPorts[clientIndex] = addr->sin_port;
	inet_ntop(AF_INET, &addr->sin_addr, clientIPStrings[clientIndex], INET_ADDRSTRLEN);

	sprintf(clientIDStrings[clientIndex],
			"%s%d",
			CLIENT_ID_PREFIX_STRING,
			clientIndex);

	displayNewClientConnectedMsg(clientIndex);
	send(connectionSockets[clientIndex], clientIDStrings[clientIndex], INET_ADDRSTRLEN, 0);

	// TODO: protect shared memory -- cmdResultsBuffer and cmdRecvingBuffer
	// pthread_mutex_lock(&communicationBuffersMutex);
	while(recv(connectionSockets[clientIndex], cmdRecvingBuffer, CMD_BUFFER_SIZE, 0) != -1) {
		bzero(cmdResultsBuffer, CMD_BUFFER_SIZE);
		executeFileserverCommandAsClient(clientIndex);
		bzero(cmdRecvingBuffer, CMD_BUFFER_SIZE);

		send(connectionSockets[clientIndex], cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
	}

	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
	printf("Initializing server...\n");

	if (argc == 1) {
		serverPort = DEFAULT_SERVER_PORT;
		printf("Port no. not specified, using default: %d\n", DEFAULT_SERVER_PORT);

	} else if (argc == 2) {
		serverPort = atoi(argv[1]);

	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_CODE_ERROR);
	}
	signal(SIGINT, willApplicationTerminate); // Close all sockets and exit when application received SIGTERM

	printf("Port no: %d\n", serverPort);

	listenSocket = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
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
			strcpy(cmdResultsBuffer, MSG_ONLY_SUPPORT_IPV4);
			send(newConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
			close(newConnectionSocket);
		}
		else if (numberOfConnectedClients == MAX_NUM_CONNECTED_CLIENTS) {
		// else if (nextClientIndexToAllocate() == NO_MORE_CLIENTS_SHOULD_CONNECT) {
			strcpy(cmdResultsBuffer, MSG_MAX_NUM_CLIENTS_REACHED);
			send(newConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
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
