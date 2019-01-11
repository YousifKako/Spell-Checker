#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define TRUE 1
#define FALSE 0

#define DEFAULT_DICTIONARY "Dictionary.txt"
#define DEFAULT_PORT "8884"
#define ADDRESS "127.0.0.1"
#define NUM_SOCK_CLIENTS 1  /* NUMBER OF CLIENTS THAT CAN CONNECT */
#define NUM_WORKS 1         /* NUMBER OF THREADS */
#define MAX 1024
#define BACKLOG 10

char** dictionary;
int serverSocket;
pthread_mutex_t logMutex;

typedef struct
{
    int* buf;
    int capacity;
    int occupiedSlots;
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t emptySlot;
    pthread_cond_t fullSlot;
} SbufMonitor;

void initSbufMonitor(SbufMonitor* sp)
{
    sp->buf      = (int *)calloc(NUM_SOCK_CLIENTS, sizeof(int));
    sp->capacity = NUM_SOCK_CLIENTS;
    sp->front    = sp->rear = 0;
    sp->size     = 0;
    pthread_mutex_init(&sp->mutex, NULL);
    pthread_mutex_init(&logMutex, NULL);    // Logging Thread Lock
    pthread_cond_init(&sp->emptySlot, NULL);
    pthread_cond_init(&sp->fullSlot, NULL);
}

void insertSbufMonitor(SbufMonitor* sp, int socket)
{
    pthread_mutex_lock(&sp->mutex);
    
    while (sp->size == sp->capacity)
    {
        pthread_cond_wait(&sp->emptySlot, &sp->mutex);
    }

    sp->size++;
    sp->buf[(++sp->rear)%(sp->capacity)] = socket;
    pthread_cond_signal(&sp->fullSlot);
    pthread_mutex_unlock(&sp->mutex);
}

int removeSbufMonitor(SbufMonitor* sp)
{
    int socket;

    pthread_mutex_lock(&sp->mutex);
    
    while (sp->size == 0)
    {
        pthread_cond_wait(&sp->fullSlot, &sp->mutex);
    }
    
    sp->size--;
    socket = sp->buf[(++sp->front)%(sp->capacity)];
    pthread_cond_signal(&sp->emptySlot);
    pthread_mutex_unlock(&sp->mutex);

    return socket;
}

int countFileLines(const char* filename)
{
    char chr;
    FILE* file = fopen(filename, "r");
    int* count = (int *)malloc(sizeof(int));

    /*
    Count Number of Lines in the Dictionary File
    to Assist With the loadDictionary Function
    */
    while(!feof(file))
    {
        chr = fgetc(file);
        if (chr == '\n')
            (*count)++;
    }

    return *count;
}

char** loadDictionary(const char* filename, int count)
{
    FILE* dictionaryFile = fopen(filename, "r");
    char** _dictionary   = (char **)malloc(sizeof(char *) * (count));
    char* buff           = NULL;
    int i                = 0;
    size_t len           = 0;
    size_t numChar;
    
    // Load Dictionary to a List
    while (numChar = getline(&buff, &len, dictionaryFile))
    {
        buff[numChar - 1] = '\0';
        _dictionary[i]    = buff;
        i++;
        if (i == count)
            break;
            
        buff = NULL;
    }

    return _dictionary;
}

int8_t contains(const char* buff)
{
    // Check if buff is in the Dictionary
    for (int i = 0; dictionary[i] != NULL; ++i)
    {
        if (!strcmp(dictionary[i], buff))
        {
            // Word Found
            return 1;
        }
    }

    // Word Was Not Found
    return 0;
}

void logging(const char* buff, const char* result)
{
    // Lock the Following Code
    pthread_mutex_lock(&logMutex);
    
    // Open Log File as Append
    FILE* logFile = fopen("log.txt", "a");
    // Write to File
    fprintf(logFile, "Word: %s | Result: %s\n", buff, result);
    // Close File
    fclose(logFile);

    pthread_mutex_unlock(&logMutex);
    // Unlock
}

void sendBuff(int socket, const char* buff, size_t len)
{
    // Send to Client
    send(socket, buff, len, 0);
}

void* receiver(void* args)
{
    // Cast args to the Struct
    SbufMonitor* tmp = (SbufMonitor *)args;
    // Remove from List
    int socket       = removeSbufMonitor(tmp);

    char buff[MAX];
    ssize_t buffLen;

    while (TRUE)
    {
        sendBuff(socket, "Enter a Word: ", 14);
        
        // Wait to Receive Data
        buffLen = recv(socket, buff, MAX, 0);
        // Receive Success Check
        if(buffLen < 0)
        {
            fprintf(stderr, "Error Occured While Receiving Data...\n");
            exit(EXIT_FAILURE);
        }
        // Check if Client Disconnected With Keyboard Interrupt
        if (buffLen == 0)
        {
            close(socket);
            printf("Client Has Disconnected\n");
            socket = removeSbufMonitor(tmp);
        }

        // Remove NewLine Character
        buff[buffLen - 1] = '\0';
        // If Word is in the Dictionary
        int8_t result     = contains(buff);
        
        if (result == 0)
        {
            sendBuff(socket, "MISSPELLED\n", 11);
            logging(buff, "MISSPELLED");
        }
        else
        {
            sendBuff(socket, "OK\n", 3);
            logging(buff, "OK");
        }
    }
}

void acceptClients(int mySocket, SbufMonitor* sp)
{
    printf("Waiting for Clients...\n");
    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);
    // Waiting for Clients to Connect
    int clientSocket = accept(mySocket, (struct sockaddr *)&client, &clientLen);

    if (clientSocket < 0)
    {
        fprintf(stderr, "Was Not Able to Accept Connection\n");
        exit(EXIT_FAILURE);
    }

    // Client Gets Added to the List
    insertSbufMonitor(sp, clientSocket);
    printf("Client Has Connected\n");
}

int createServer(const char* port)
{
    // Initilizing the Servers Family(TCP), Address, and Port
    struct sockaddr_in server;
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = inet_addr(ADDRESS);
    // Convert Port to int
    int portAsInt          = atoi(port);
    server.sin_port        = htons(portAsInt);
    
    // Create Server Socket
    int mySocket = socket(AF_INET, SOCK_STREAM, 0);

    // Socket Success Check
    if (mySocket < 0)
    {
        fprintf(stderr, "Unable to Create Socket\n");
        exit(EXIT_FAILURE);
    }
    // Bind Success Check
    if (bind(mySocket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        fprintf(stderr, "Unable to Bind to Socket\n");
        exit(EXIT_FAILURE);
    }
    // Listen Success Check
    if (listen(mySocket, BACKLOG) < 0)
    {
        fprintf(stderr, "Listen Error. Quitting.\n");
        close(mySocket);
        exit(EXIT_FAILURE);
    }

    // Return Server Socket
    return mySocket;
}

pthread_t* initThreadPool(SbufMonitor* sp)
{
    // Create NUM_WORKS Amount of Workers and Initialize Them to 0
    pthread_t* threadID = (pthread_t *)calloc(NUM_WORKS, sizeof(pthread_t));

    for (int i = 0; i < NUM_WORKS; ++i)
    {
        pthread_create(&threadID[i], NULL, receiver, (void *)sp);
    }

    return threadID;
}

void interrupt()
{
    // This Function is Waiting for a Ctrl+C Signl
    fprintf(stderr, "\nInterrupt Exiting...\n");
    close(serverSocket);
}

int main(int argc, char* argv[])
{
    if (argc <= 3)
    {
        char* port;
        int count;

        // Default Settings
        if (argc == 1)
        {
            // Default Port
            port       = DEFAULT_PORT;
            count      = countFileLines(DEFAULT_DICTIONARY);
            dictionary = loadDictionary(DEFAULT_DICTIONARY, count);
        }
        // Only Dictionary is Given
        else if (argc == 2)
        {
            port       = DEFAULT_PORT;
            count      = countFileLines(argv[1]);
            dictionary = loadDictionary(argv[1], count);
        }
        // Dictionary and Port are Available
        else
        {
            // Given Port
            port       = argv[2];
            count      = countFileLines(argv[1]);
            dictionary = loadDictionary(argv[1], count);
        }
        
        // Number of Clients that Can Connect
        SbufMonitor* sp     = (SbufMonitor *)malloc(sizeof(SbufMonitor));
        initSbufMonitor(sp);

        // Create Worker Threads
        pthread_t* threadID = initThreadPool(sp);

        // If Ctrl+C is Pressed, the Server Will Terminate Safely
        signal(SIGTERM, interrupt);

        // Create Server
        serverSocket        = createServer(port);
        
        // Accept Connections
        while (TRUE)
        {
            acceptClients(serverSocket, sp);
        }
    }
    // Too Many Arguments
    else
    {
        fprintf(stderr, "Too Many Arguments\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}