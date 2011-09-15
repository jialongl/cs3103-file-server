#define DEFAULT_SERVER_PORT 6789
#define MAX_NUM_PENDING_CONNECTIONS 1024

#define MAX_NUM_SHARED_FILE_RECORDS 100
#define MAX_NUM_TOKENS 3
#define MAX_TOKEN_LENGTH 32

typedef struct {
	int id;
	char owner[INET_ADDRSTRLEN]; // the file could be on "server", "c1", "c2" etc.
	char filename[MAX_TOKEN_LENGTH];
} SharedFileRecord;
