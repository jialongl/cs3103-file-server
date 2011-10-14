#define EXIT_CODE_ERROR 500 // 500: Internal server error.... :p
#define EXIT_CODE_CLEAN 200 // 200: OK

#define CMD_BUFFER_SIZE 1024
#define FILE_TRANSMISSION_BUFFER_SIZE 1024

#define COMMAND_HELP "help"
#define COMMAND_LIST_AVAILABLE_FILES "list"
#define COMMAND_LIST_MY_SHARED_FILES "shared"
#define COMMAND_REGISTER_FILE "reg"
#define COMMAND_DOWNLOAD_FILE "get"
#define COMMAND_QUIT "quit"

#define MSG_MAX_NUM_CLIENTS_REACHED "server doesn't server more than 10 clients"
#define MSG_ONLY_SUPPORT_IPV4 "only support IPv4 connection"

#define MAX_CLIENT_ID_STRLEN 12
#define MAX_COMMAND_TOKEN_STRLEN 32

typedef struct {
	int vacant;
	char owner[MAX_CLIENT_ID_STRLEN]; // the file could be on "server", "c1", "c2" etc.
	char filename[MAX_COMMAND_TOKEN_STRLEN];
} SharedFileRecord;

typedef struct {
	int vacant;
	char fileHostIPAddr[INET_ADDRSTRLEN];
	int  fileHostPort;
	char filename[MAX_COMMAND_TOKEN_STRLEN];
} RequestedFileInfo;

#define DEFAULT_DIRECTORY_TO_SHARE "./shared/"

#define DEFAULT_SERVER_COMMAND_LISTENING_PORT 51927
#define DEFAULT_SERVER_FILE_TRANSMISSION_LISTENING_PORT 2915
#define DEFAULT_CLIENT_FILE_TRANSMISSION_LISTENING_PORT 62915

#define MAX_NUM_PENDING_CONNECTIONS 1024

#define MAX_NUM_SHARED_FILE_RECORDS 100
#define MAX_NUM_CONNECTED_CLIENTS 10
#define MAX_NUM_PARALLEL_DOWNLOADS 5

#define CLIENT_ID_PREFIX_STRING "client"

#define MAX_NUM_COMMAND_TOKENS 3

#define NO_VACANCIES -1
#define IN_USE 1
#define VACANT 0
