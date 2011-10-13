#define EXIT_CODE_ERROR 500 // 500: Internal server error.... :p
#define EXIT_CODE_CLEAN 200 // 200: OK

#define CMD_BUFFER_SIZE 1024

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
	short valid;
	char owner[MAX_CLIENT_ID_STRLEN]; // the file could be on "server", "c1", "c2" etc.
	char filename[MAX_COMMAND_TOKEN_STRLEN];
} SharedFileRecord;
