#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include "queue.h"  // Queue utility methods
#include "aesd_ioctl.h"

// Define constants
#define MAX_BACKLOG 10
#define BUFFER_SIZE 100
const char *ioctl_string = "AESDCHAR_IOCSEEKTO:";

// Declare global variables
char *port = "9000";

#if USE_AESD_CHAR_DEVICE == 1
char *dataFile = "/dev/aesdchar";
#else
char *dataFile = "/var/tmp/aesdsocketdata";
#endif

// Descriptors
int socketDescriptor = 0;
int socketAcceptDescriptor = 0;
int fileDescriptor = 0;

// Counters
volatile int data_count = 0; // Marked as volatile for thread safety

// Function prototypes
void ConfigureSocket(void);
void *threadHandler(void *threadParam);
void exitFunction(void);
void signalHandler(int);

// Flags
int daemonFlag = 0;
volatile bool doneFlag = false; // Marked as volatile for thread safety

// Structure to hold thread parameters
typedef struct
{
    bool isDone;
    pthread_t ID;
    int threadFD;
} ThreadsDef;

// Structure for linked list data
struct ListDataDef
{
    ThreadsDef socketThread;
    SLIST_ENTRY(ListDataDef) entries;
};

typedef struct ListDataDef ListData_t;
ListData_t *dataPointer = NULL;
pthread_mutex_t mutexLock = PTHREAD_MUTEX_INITIALIZER;
SLIST_HEAD(slistHead, ListDataDef) head;

int main(int argc, char *argv[])
{
    // Initialize syslog
    openlog("Assignment6-Part1", LOG_PID, LOG_USER);
    syslog(LOG_DEBUG, "Openlog ... PASS");

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGKILL, signalHandler);

    // Initialize mutex
    pthread_mutex_init(&mutexLock, NULL);

    // Check if daemon mode is enabled
    if ((argc > 1) && (!strcmp("-d", (char *)argv[1])))
    {
        printf("Daemon mode ... \n");
        syslog(LOG_DEBUG, "aesdsocket daemon mode");
        daemonFlag = 1;
    }

    // Establish socket connection
    ConfigureSocket();

    // Close syslog
    closelog();

    return 0;
}

// Signal handler function
void signalHandler(int signalNumber)
{
    if (signalNumber == SIGINT || signalNumber == SIGTERM || signalNumber == SIGKILL)
    {
        syslog(LOG_DEBUG, "SIG ... Exiting");
        shutdown(socketDescriptor, SHUT_RDWR);
        doneFlag = true;
        unlink(dataFile);
        close(socketAcceptDescriptor);
        close(socketDescriptor);
        _exit(0);
    }
}

// Get the socket up
void ConfigureSocket()
{
    // Variables
    struct addrinfo hints;
    struct addrinfo *res;
    struct sockaddr clientAddr;
    socklen_t clientSize;

    // Initialize linked list
    SLIST_INIT(&head);

    // Set up address hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(NULL, port, &hints, &res);
    // Create socket
    socketDescriptor = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    bind(socketDescriptor, res->ai_addr, res->ai_addrlen);
    // Create data file
    fileDescriptor = creat(dataFile, 0644);
    close(fileDescriptor);

    // Free address info
    freeaddrinfo(res);

    // If daemon mode is enabled
    if (daemonFlag == 1)
    {
        int tempDaemon = daemon(0, 0);
        if (tempDaemon == -1)
        {
            syslog(LOG_ERR, "Daemon mode failed %s", strerror(errno));
        }
    }

    while (doneFlag == false)
    {
        listen(socketDescriptor, MAX_BACKLOG);
        clientSize = sizeof(struct sockaddr);

        // Accept connection
        socketAcceptDescriptor = accept(socketDescriptor, &clientAddr, &clientSize);
        struct sockaddr_in *addr_in = (struct sockaddr_in *)&clientAddr;
        inet_ntoa(addr_in->sin_addr);
        dataPointer = (ListData_t *)malloc(sizeof(ListData_t));

        // Insert into list
        SLIST_INSERT_HEAD(&head, dataPointer, entries);
        dataPointer->socketThread.threadFD = socketAcceptDescriptor;
        dataPointer->socketThread.isDone = false;

        // Create a pthread
        pthread_create(&(dataPointer->socketThread.ID), NULL, threadHandler, &(dataPointer->socketThread));

        // Remove stale thread references
        SLIST_FOREACH(dataPointer, &head, entries)
        {
            if (dataPointer->socketThread.isDone == true)
            {
                pthread_join(dataPointer->socketThread.ID, NULL);
                SLIST_REMOVE(&head, dataPointer, ListDataDef, entries);
                free(dataPointer);
                break;
            }
        }
    }

    close(socketAcceptDescriptor);
    close(socketDescriptor);
}

// All things threads
void *threadHandler(void *threadParam)
{
    // Variables
    bool packetComplete = false;
    int retRecv = 0;
    char buffer[BUFFER_SIZE] = {0};
    char *outputBuffer = NULL;
    char *sendBuffer = NULL;
    ThreadsDef *params = (ThreadsDef *)threadParam;
    int i, j = 0;

    outputBuffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    memset(outputBuffer, 0, BUFFER_SIZE);

    while (packetComplete == false)
    {
        retRecv = recv(params->threadFD, buffer, BUFFER_SIZE, 0);
        if (retRecv < 0)
        {
            syslog(LOG_ERR, "Socket receive error %s.", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (retRecv == 0)
        {
            break;
        }

        // Accept till '\n'
        for (i = 0; i < BUFFER_SIZE; i++)
        {
            printf("Before");
            if (buffer[i] == '\n')
            {
                printf("Inside");
                packetComplete = true;
                i++;
                break;
            }
        }

        j += i;
        data_count += i;

        outputBuffer = (char *)realloc(outputBuffer, (j + 1));
        strncat(outputBuffer, buffer, i + 1);
        memset(buffer, 0, BUFFER_SIZE);
    }

    // Start lock
    pthread_mutex_lock(&mutexLock);
    FILE *fileFD = fopen(dataFile, "a+");

    // Check if we have got an IOCSEEKTO
    if (strncmp(outputBuffer, ioctl_string, strlen(ioctl_string)) == 0)
    {
        // IOCTL call with params
        struct aesd_seekto seek_params;
        sscanf(outputBuffer, "AESDCHAR_IOCSEEKTO:%d,%d", &seek_params.write_cmd, &seek_params.write_cmd_offset);

        // Get the file number to call IOCTL
        int file_number = fileno(fileFD);
        ioctl(file_number, AESDCHAR_IOCSEEKTO, &seek_params);
    }
    else
    { // Regular
        // Write buffer to file
        fwrite(outputBuffer, 1, strlen(outputBuffer), fileFD);
        // The specs mention not to close the file, we'll rewind to send the entire contents back
        rewind(fileFD);
    }

    // Send back the contents of the file
    int bytes_read = 0;
    sendBuffer = (char *)malloc(sizeof(char) * (1));

    // May not be very efficient but works good for our data sizes
    while ((bytes_read = fread(sendBuffer, 1, 1, fileFD)) > 0)
    {
        send(params->threadFD, sendBuffer, bytes_read, 0);
    }

    // Close
    fclose(fileFD);
    params->isDone = true;

    // Release memory
    free(outputBuffer);
    free(sendBuffer);

    // Unlock mutex
    pthread_mutex_unlock(&mutexLock);

    return params;
}

// Clean up to end
void exitFunction(void)
{
#if USE_AESD_CHAR_DEVICE != 1
    // Get rid of the datafile
    // unlink(dataFile);
#endif

    // Close all files
    close(fileDescriptor);
    close(socketAcceptDescriptor);
    close(socketDescriptor);

    // Clean up the list and threads
    while (SLIST_FIRST(&head) != NULL)
    {
        SLIST_FOREACH(dataPointer, &head, entries)
        {
            close(dataPointer->socketThread.threadFD);
            pthread_join(dataPointer->socketThread.ID, NULL);
            SLIST_REMOVE(&head, dataPointer, ListDataDef, entries);
            free(dataPointer);
            break;
        }
    }

#if USE_AESD_CHAR_DEVICE != 1
    // Join the timer thread
    if (timerThread)
    {
        pthread_join(timerThread, NULL);
    }
#endif

    // Go out
    exit(EXIT_SUCCESS);
}

