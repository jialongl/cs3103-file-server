#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "constants.h"

int cmdListeningSocket;
int cmdConnectionSockets[MAX_NUM_CONNECTED_CLIENTS];
int newCmdConnectionSocket; // used as a temp.

int dataListeningSocket;
int dataConnectionSockets[MAX_NUM_CONNECTED_CLIENTS];
int newDataConnectionSocket; // used as a temp.

int serverPort;
int clientPorts[MAX_NUM_CONNECTED_CLIENTS];
struct sockaddr_in serverSockaddr;
struct sockaddr_in dataServerSockaddr;
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
pthread_mutex_t cmdBuffersMutex = PTHREAD_MUTEX_INITIALIZER;

void* fileSendingBuffer[FILE_TRANSMISSION_BUFFER_SIZE];

// EFFECTS: closes all sockets this application uses, and exits.
void willApplicationTerminate () {
	printf("Server received SIGINT, exiting...\n");
	for (int i=0; i<MAX_NUM_CONNECTED_CLIENTS; i++) {
		if (cmdConnectionSockets[i] != -1)
			close(cmdConnectionSockets[i]);
	}
	close(cmdListeningSocket);
	close(dataListeningSocket);
	exit(EXIT_CODE_CLEAN);
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
		if (sharedFileRecords[i].vacant == VACANT)
			return i;
	}
	return NO_VACANCIES;
}

int nextClientIndexToAllocate() {
	for (int i=0; i<MAX_NUM_CONNECTED_CLIENTS; i++)
		if (cmdConnectionSockets[i] == -1)
			return i;
	return NO_VACANCIES;
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

char* substr(const char *str, int startIndex, int numChars) {
	char* newStr = malloc(numChars+1);
	strncpy(newStr, str+startIndex, numChars);
	newStr[numChars] = '\0';
	return newStr;
}

void sendFileToClient (int fileIndex, int clientIndex) {
	// int fileTransmissionSocket = socket(AF_INET, SOCK_STREAM, 0);
	// connect(fileTransmissionSocket,
	// 		(struct sockaddr*) &(clientSockaddrs[clientIndex]),
	// 		sizeof(struct sockaddr_in));
	
}

void recvFileFromClient (int fileIndex, int clientIDString) {

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
			if (sharedFileRecords[i].vacant == VACANT) continue;
			sprintf(sprintfBuffer, "F%d:\t%s\t%s\n",
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
			if (sharedFileRecords[i].vacant == VACANT) continue;
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
				if (fileIndex == NO_VACANCIES) {
					strcpy(cmdResultsBuffer, "Sorry, no more files can be registered at this time. Wait till some clients disconnect.");

				} else {
					sharedFileRecords[fileIndex].vacant = IN_USE;
					strcpy(sharedFileRecords[fileIndex].owner, clientIDStrings[clientIndex]);
					strcpy(sharedFileRecords[fileIndex].filename, tokens[i]);

					pthread_mutex_lock(&fileRecordsCounterMutex);
					numberOfRecords++;
					pthread_mutex_unlock(&fileRecordsCounterMutex);
				}
			}
		}
	}
	else if (strncmp(cmdRecvingBuffer, COMMAND_DOWNLOAD_FILE, strlen(COMMAND_DOWNLOAD_FILE)) == 0) {

		char tokens[MAX_NUM_COMMAND_TOKENS][MAX_COMMAND_TOKEN_STRLEN];
		int numberOfTokens;
		char sprintfBuffer[sizeof(SharedFileRecord)];
		tokenizeString(cmdRecvingBuffer, " \n", tokens, &numberOfTokens);

		for (int i=1; i<numberOfTokens; i++) {
			if (tokens[i][0] != 'F') continue;

			int fileIndex = atoi(substr(tokens[i], 1, strlen(tokens[i])-1));
			if (sharedFileRecords[fileIndex].vacant == VACANT) {
				sprintf(sprintfBuffer, "F%d: %s doesn't exist on the server.\n",
						fileIndex,
						sharedFileRecords[i].filename);
				strncat(cmdResultsBuffer, sprintfBuffer, CMD_BUFFER_SIZE);
				continue;
			}
			else if (strcmp(sharedFileRecords[fileIndex].owner, "server") == 0) {
				// send info to client about which host this file is on --
				// who to connect() and download.
				RequestedFileInfo info;
				info.vacant = IN_USE;
				strcpy(info.filename, sharedFileRecords[fileIndex].filename);
				info.fileHostPort = DEFAULT_SERVER_FILE_TRANSMISSION_LISTENING_PORT;
				strcpy(info.fileHostIPAddr, "serverip");
				memcpy(cmdResultsBuffer, &info, sizeof(info));
			}
			else { // the file is on another client
				// send info about who to connect() and download.
				RequestedFileInfo info;
				info.vacant = IN_USE;
				strcpy(info.filename, sharedFileRecords[fileIndex].filename);
				info.fileHostPort = DEFAULT_CLIENT_FILE_TRANSMISSION_LISTENING_PORT;
				strcpy(info.fileHostIPAddr, clientIPStrings[clientIndex]);
				memcpy(cmdResultsBuffer, &info, sizeof(info));
			}
		}
	}
	else if (((strncmp(cmdRecvingBuffer, COMMAND_QUIT, strlen(COMMAND_QUIT)) == 0) && (cmdRecvingBuffer[strlen(COMMAND_QUIT)] == '\n')) ||
			 (strlen(cmdRecvingBuffer) == 0)) {
		// using "quit" command or Ctrl-D

		printf("Client \"%s\" quitting...\n", clientIDStrings[clientIndex]);

		char namesOfFilesToBeErased[MAX_NUM_SHARED_FILE_RECORDS * MAX_COMMAND_TOKEN_STRLEN];
		memset(namesOfFilesToBeErased, 0, sizeof(namesOfFilesToBeErased));

		// "clean up" files registered by that client
		for (int i=0; i<numberOfRecords; i++) {
			if (strcmp(sharedFileRecords[i].owner, clientIDStrings[clientIndex]) == 0) {
				sharedFileRecords[i].vacant = VACANT;
				strncat(namesOfFilesToBeErased, sharedFileRecords[i].filename, strlen(sharedFileRecords[i].filename));
				strncat(namesOfFilesToBeErased, ", ", 2);
			}
		}
		if (namesOfFilesToBeErased[0] == '\0')
			strcpy(namesOfFilesToBeErased, "none");
		printf("Erased shared files from server list: %s\n", namesOfFilesToBeErased);

		close(cmdConnectionSockets[clientIndex]);
		cmdConnectionSockets[clientIndex] = -1; // mark as "released"/"re-usable"

		pthread_mutex_lock(&connectedClientsCounterMutex);
		numberOfConnectedClients--;
		pthread_mutex_unlock(&connectedClientsCounterMutex);
	}
	else {
		strcpy(cmdResultsBuffer, "command not recognized.\n");
	}
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
	send(cmdConnectionSockets[clientIndex], clientIDStrings[clientIndex], INET_ADDRSTRLEN, 0);

	// TODO: protect shared memory -- cmdResultsBuffer and cmdRecvingBuffer
	// pthread_mutex_lock(&cmdBuffersMutex);
	while(recv(cmdConnectionSockets[clientIndex], cmdRecvingBuffer, CMD_BUFFER_SIZE, 0) != -1) {
		bzero(cmdResultsBuffer, CMD_BUFFER_SIZE);
		executeFileserverCommandAsClient(clientIndex);
		bzero(cmdRecvingBuffer, CMD_BUFFER_SIZE);

		send(cmdConnectionSockets[clientIndex], cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
	}

	pthread_exit(NULL);
}

void registerFilesSharedByServer() {
	DIR *dir;
	struct dirent *ent;
	dir = opendir ("./shared/");
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if (strcmp(ent->d_name, "..") == 0 ||
				strcmp(ent->d_name, ".") == 0)
				continue;

			int fileIndex = nextFileRecordIndexToAllocate();

			if (fileIndex == NO_VACANCIES) {
				printf("There are too many files in directory \"%s\". This program only tracks %d files. Delete some files in \"%s\" or modify MAX_NUM_SHARED_FILE_RECORDS in server_constants.h and re-make.\n", DEFAULT_DIRECTORY_TO_SHARE, MAX_NUM_SHARED_FILE_RECORDS, DEFAULT_DIRECTORY_TO_SHARE);

			} else {
				sharedFileRecords[fileIndex].vacant = IN_USE;
				strcpy(sharedFileRecords[fileIndex].owner, "server");
				strcpy(sharedFileRecords[fileIndex].filename, ent->d_name);

				pthread_mutex_lock(&fileRecordsCounterMutex);
				numberOfRecords++;
				pthread_mutex_unlock(&fileRecordsCounterMutex);
			}
		}
		closedir (dir);

	} else {
		/* could not open directory */
		printf("Can't open directory \"%s\". Therefore, there are no shared files on server.\n", DEFAULT_DIRECTORY_TO_SHARE);
	}
}

void* dataConnection();

int main(int argc, char* argv[]) {
	printf("Initializing server...\n");

	if (argc == 1) {
		serverPort = DEFAULT_SERVER_COMMAND_LISTENING_PORT;
		printf("Port no. not specified, using default: %d\n", DEFAULT_SERVER_COMMAND_LISTENING_PORT);

	} else if (argc == 2) {
		serverPort = atoi(argv[1]);

	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	printf("Port no: %d\n", serverPort);

	for (int i=0; i<MAX_NUM_SHARED_FILE_RECORDS; i++)
		sharedFileRecords[i].vacant = VACANT;

	registerFilesSharedByServer();
	signal(SIGINT, willApplicationTerminate); // Close all sockets and exit when application received SIGTERM


	pthread_t dataConnectionThread;
	if (pthread_create(&dataConnectionThread, NULL, dataConnection, NULL) != 0)
		perror("pthread_create() when creating data connection");

	cmdListeningSocket = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
	if (cmdListeningSocket == -1) {
		fprintf(stderr, "Error with socket().\n");
		exit(EXIT_CODE_ERROR);
	}

	memset(cmdConnectionSockets, -1, sizeof(cmdConnectionSockets));

	serverSockaddr.sin_family = AF_INET;
	serverSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverSockaddr.sin_port = htons(serverPort);
	if (bind(cmdListeningSocket,
			 (struct sockaddr *) &serverSockaddr,
			 sizeof(serverSockaddr)) == -1) {
		perror("bind()");
		// fprintf(stderr, "Error with bind().\n");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(cmdListeningSocket, MAX_NUM_PENDING_CONNECTIONS)) {
		perror("listen()");
		// fprintf(stderr, "Error with listen().\n");
		exit(EXIT_CODE_ERROR);
	}
	printf("Listening...\n");

	// start to run forever for accept-communicate-close cycle.
	socklen_t len = sizeof(struct sockaddr_storage);
	while (1) {
		newCmdConnectionSocket = accept(cmdListeningSocket, (struct sockaddr*) &newClientSockaddr, &len);

		if (newCmdConnectionSocket == -1) {
			perror("accept()");
			// fprintf(stderr, "Error with accept().\n");
			exit(EXIT_CODE_ERROR);
		}
		else if (newClientSockaddr.ss_family != AF_INET) {
			strcpy(cmdResultsBuffer, MSG_ONLY_SUPPORT_IPV4);
			send(newCmdConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
			close(newCmdConnectionSocket);
		}
		else if (numberOfConnectedClients == MAX_NUM_CONNECTED_CLIENTS) {
		// else if (nextClientIndexToAllocate() == NO_VACANCIES) {
			strcpy(cmdResultsBuffer, MSG_MAX_NUM_CLIENTS_REACHED);
			send(newCmdConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
			close(newCmdConnectionSocket);
		}
		else {
			// new TCP connection with a client established.
			int clientIndex = nextClientIndexToAllocate();
			cmdConnectionSockets[clientIndex] = newCmdConnectionSocket;
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

void* dataConnection() {
	dataListeningSocket = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
	if (dataListeningSocket == -1) {
		fprintf(stderr, "Error with socket().\n");
		exit(EXIT_CODE_ERROR);
	}

	memset(dataConnectionSockets, -1, sizeof(dataConnectionSockets));

	dataServerSockaddr.sin_family = AF_INET;
	dataServerSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	dataServerSockaddr.sin_port = htons(DEFAULT_SERVER_FILE_TRANSMISSION_LISTENING_PORT);
	if (bind(dataListeningSocket,
			 (struct sockaddr *) &dataServerSockaddr,
			 sizeof(dataServerSockaddr)) == -1) {
		perror("bind()");
		exit(EXIT_CODE_ERROR);
	}

	if (listen(dataListeningSocket, MAX_NUM_PENDING_CONNECTIONS)) {
		perror("listen()");
		exit(EXIT_CODE_ERROR);
	}

	socklen_t len = sizeof(struct sockaddr_storage);
	while (1) {
		newDataConnectionSocket = accept(dataListeningSocket, (struct sockaddr*) &newClientSockaddr, &len);

		recv(newDataConnectionSocket, fileSendingBuffer, FILE_TRANSMISSION_BUFFER_SIZE, 0);
		RequestedFileInfo info;
		memcpy(&info, fileSendingBuffer, sizeof(RequestedFileInfo));

		char filepath[MAX_COMMAND_TOKEN_STRLEN];
		memset(filepath, '\0', sizeof(filepath));
		strcat(filepath, DEFAULT_DIRECTORY_TO_SHARE);
		strcat(filepath, info.filename); //, strlen(info.filename));

		printf("filepath=%s", filepath);
		FILE *requestedFile = fopen(filepath, "r");

		int numBytesRead = fread(fileSendingBuffer, sizeof(char), FILE_TRANSMISSION_BUFFER_SIZE, requestedFile);

		while( numBytesRead == FILE_TRANSMISSION_BUFFER_SIZE ) {
			if( (ferror(requestedFile)) || feof(requestedFile) ) break;

			send(newDataConnectionSocket, fileSendingBuffer, FILE_TRANSMISSION_BUFFER_SIZE, 0);
			numBytesRead = fread(fileSendingBuffer, sizeof(char), FILE_TRANSMISSION_BUFFER_SIZE, requestedFile);
		}
		// send the last bits
		send(newDataConnectionSocket, fileSendingBuffer, numBytesRead, 0);
		fclose(requestedFile);
	}
	pthread_exit(NULL);
}
