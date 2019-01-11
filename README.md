NOTE: If you would like to exit the client or the server, you may do so with Ctrl+C
      A proper disconnet/termination will take part.

void initSbufMonitor(SbufMonitor* sp)
	This initilizes monitor struct

void insertSbufMonitor(SbufMonitor* sp, int socket)
	Insert to monitor struct

int removeSbufMonitor(SbufMonitor* sp)
	Remove from monitor struct

int countFileLines(const char* filename)
	This function returns the number of lines

char** loadDictionary(const char* filename, int count)
	This function loads the dictionary into an array
	for O(n) access time

int8_t contains(const char* buff)
	This function checks the buffer that was sent from the client to
	the server to be checked if it was spelled correctly or not

void logging(const char* buff, const char* result)
	This function logs to a file

void sendBuff(int socket, const char* buff, size_t len)
	This function sends OK if the buffer was spelled correctly and
	MISSPELLED if it was not

void* receiver(void* args)
	This function receives a buffer from a client

void acceptClients(int mySocket, SbufMonitor* sp)
	This function accepts connections from clients

int createServer(const char* port)
	This function creates the server to listen to clients

pthread_t* initThreadPool(SbufMonitor* sp)
	This function initilizes the pool of threads
