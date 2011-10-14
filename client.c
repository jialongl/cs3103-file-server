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

#include "constants.h"


int fileTransmissionListenSocket;
struct sockaddr_in fileTransmissionSockaddr;

RequestedFileInfo infos[MAX_NUM_PARALLEL_DOWNLOADS];
int numberOfParallelDownloads = 0;
pthread_mutex_t parallelDownloadsCounterMutex = PTHREAD_MUTEX_INITIALIZER;

int cmdConnectionSocket;
int serverPort, clientPort;
char clientIDString[MAX_CLIENT_ID_STRLEN];

char *serverIPString;
struct sockaddr_in serverSockaddr;
char cmdSendingBuffer[CMD_BUFFER_SIZE];
char cmdResultsBuffer[CMD_BUFFER_SIZE];

void* fileSendingBuffer[FILE_TRANSMISSION_BUFFER_SIZE];
void* fileRecvingBuffer[FILE_TRANSMISSION_BUFFER_SIZE];


void willApplicationTerminate() {
	printf("Client received SIGINT, exiting...\n");
	close(cmdConnectionSocket);
	exit(EXIT_CODE_CLEAN);
}

int nextFileRequestInfoIndexToAllocate() {
	for (int i=0; i<MAX_NUM_PARALLEL_DOWNLOADS; i++) {
		if (infos[i].vacant == VACANT)
			return i;
	}
	return NO_VACANCIES;
}

void connectToServer() {
	cmdConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);

	serverSockaddr.sin_family = AF_INET;
	serverSockaddr.sin_addr.s_addr = inet_addr(serverIPString);
	serverSockaddr.sin_port = htons(serverPort);

	if (connect(cmdConnectionSocket,
				(struct sockaddr*) &serverSockaddr,
				sizeof(serverSockaddr)) == -1) {
		perror("connect()");
		exit(EXIT_CODE_ERROR);
	}
}

void readClientIDFromServer() {
	recv(cmdConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
	sprintf(clientIDString, "%s", cmdResultsBuffer);
	bzero(cmdResultsBuffer, CMD_BUFFER_SIZE);
}

void* getFileFromRemote(void* fileInfoIndexArg) {
	int fileInfoIndex = (int) fileInfoIndexArg;

	int dataConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in peerSockaddr;
	peerSockaddr.sin_family = AF_INET;
	peerSockaddr.sin_addr.s_addr = inet_addr(infos[fileInfoIndex].fileHostIPAddr);
	peerSockaddr.sin_port = htons(infos[fileInfoIndex].fileHostPort);

	connect(dataConnectionSocket,
			(struct sockaddr*) &peerSockaddr,
			sizeof(peerSockaddr));

	// strcpy(fileSendingBuffer, infos[fileIndex].filename);
	memcpy(fileSendingBuffer, &(infos[fileInfoIndex]), sizeof(RequestedFileInfo));
	send(dataConnectionSocket, fileSendingBuffer, sizeof(RequestedFileInfo), 0);

	FILE *download = fopen(infos[fileInfoIndex].filename, "w");
	int numBytesRecved = recv(dataConnectionSocket, fileRecvingBuffer, FILE_TRANSMISSION_BUFFER_SIZE, 0);
	while(numBytesRecved == FILE_TRANSMISSION_BUFFER_SIZE) {
		fwrite(fileRecvingBuffer, sizeof(char), FILE_TRANSMISSION_BUFFER_SIZE, download);
		numBytesRecved = recv(dataConnectionSocket, fileRecvingBuffer, FILE_TRANSMISSION_BUFFER_SIZE, 0);
	}
	// write the last block of bits.
	fwrite(fileRecvingBuffer, sizeof(char), numBytesRecved, download);
	fclose(download);
	pthread_exit(NULL);
}

void* dataConnection();

int main(int argc, char* argv[]) {
	if (argc == 3) {
		serverPort = atoi(argv[2]);
		serverIPString = argv[1];

	} else {
		fprintf(stderr, "Wrong number of arguments.\n");
		exit(EXIT_CODE_ERROR);
	}

	signal(SIGINT, willApplicationTerminate);

	printf("Initializing client...\n");

	for (int i=0; i<MAX_NUM_PARALLEL_DOWNLOADS; i++)
		infos[i].vacant = VACANT;

	pthread_t dataConnectionThread;
	if (pthread_create(&dataConnectionThread, NULL, dataConnection, NULL) != 0)
		perror("pthread_create() when creating data connection");

	printf("Trying to connect to server and get a client ID...\n");
	connectToServer();
	readClientIDFromServer();

	if (strcmp(clientIDString, MSG_ONLY_SUPPORT_IPV4) == 0) {
		printf(MSG_ONLY_SUPPORT_IPV4);
		exit(EXIT_CODE_ERROR);
	}
	else if (strcmp(clientIDString, MSG_MAX_NUM_CLIENTS_REACHED) == 0) {
		printf(MSG_MAX_NUM_CLIENTS_REACHED);
		exit(EXIT_CODE_ERROR);
	}
	else {
		printf("Client ID: %s.\n", clientIDString);
		printf("Server: %s#%d\n", serverIPString, serverPort);
	}

	// start to type commands forever.
	bzero(cmdSendingBuffer, CMD_BUFFER_SIZE);
	printf("> ");

	while (fgets(cmdSendingBuffer, CMD_BUFFER_SIZE, stdin) != NULL) {
		send(cmdConnectionSocket, cmdSendingBuffer, strlen(cmdSendingBuffer), 0); // do not send bytes not inited by user input.
		short wasDownloadCommand = (strncmp(cmdSendingBuffer, COMMAND_DOWNLOAD_FILE, strlen(COMMAND_DOWNLOAD_FILE)) == 0);
		bzero(cmdSendingBuffer, CMD_BUFFER_SIZE);

		recv(cmdConnectionSocket, cmdResultsBuffer, CMD_BUFFER_SIZE, 0);
		if (wasDownloadCommand) {
			// received RequestedFileInfo from server (do not print)
			int infoIndex = nextFileRequestInfoIndexToAllocate();
			memcpy(&infos[infoIndex], cmdResultsBuffer, sizeof(RequestedFileInfo));
			if (strcmp(infos[infoIndex].fileHostIPAddr, "serverip") == 0)
				strcpy(infos[infoIndex].fileHostIPAddr, serverIPString);
			pthread_t newFileTransmissionThread;
			pthread_create(&newFileTransmissionThread, NULL, getFileFromRemote, (void*)infoIndex);

			printf("> ");
		} else
			printf("%s> ", cmdResultsBuffer);
		bzero(cmdResultsBuffer, CMD_BUFFER_SIZE);
	}

	cmdSendingBuffer[0] = '\0';
	send(cmdConnectionSocket, cmdSendingBuffer, strlen(cmdSendingBuffer), 0); // send a null string as "FYN"
	close(cmdConnectionSocket);

	return EXIT_CODE_CLEAN;
}

void* dataConnection() {
	fileTransmissionListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	fileTransmissionSockaddr.sin_family = AF_INET;
	fileTransmissionSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	fileTransmissionSockaddr.sin_port = htons(DEFAULT_CLIENT_FILE_TRANSMISSION_LISTENING_PORT);

	bind(fileTransmissionListenSocket,
		 (struct sockaddr *) &fileTransmissionSockaddr,
		 sizeof(fileTransmissionSockaddr));
	listen(fileTransmissionListenSocket, MAX_NUM_PENDING_CONNECTIONS);

	socklen_t len = sizeof(struct sockaddr_storage);
	while (1) {
		struct sockaddr_storage newPeerSockaddr;

		int newDataConnectionSocket = accept(fileTransmissionListenSocket, (struct sockaddr*) &newPeerSockaddr, &len);
		printf("new reuqest.\n");

	}
	pthread_exit(NULL);
}
