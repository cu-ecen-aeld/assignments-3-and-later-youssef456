#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <pthread.h>
#include "aesd_ioctl.h"  // Include the newly created header

#if USE_AESD_CHAR_DEVICE == 1
#include <sys/ioctl.h>
#endif

#define PORT 9000
#define MAX_PACKET_SIZE 4096

#if USE_AESD_CHAR_DEVICE == 1
#define DATA_FILE "/dev/aesdchar"
#else
#define DATA_FILE "/var/tmp/aesdsocketdata"
#endif

#define PID_FILE "/var/run/aesdsocket.pid"
#define TIMESTAMP_INTERVAL 10

int listen_socket = -1, data_fd = -1;

#if USE_AESD_CHAR_DEVICE == 1
int aesd_char_fd = -1;
#endif

pthread_mutex_t data_mutex;
pthread_mutex_t timestamp_mutex;

time_t last_timestamp;

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");

        pthread_mutex_lock(&data_mutex);

        if (data_fd != -1)
            close(data_fd);

#if USE_AESD_CHAR_DEVICE == 1
        if (aesd_char_fd != -1)
            close(aesd_char_fd);
#endif

        if (listen_socket != -1)
            close(listen_socket);

        remove(PID_FILE);

        pthread_mutex_unlock(&data_mutex);

        closelog();
        exit(EXIT_SUCCESS);
    }
}

void handle_connection(int client_socket) {
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
            syslog(LOG_ERR, "Socket rceive error %s.", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (retRecv == 0)
        {
            break;
        }

        //accept till '\n'
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

    //start lock
    pthread_mutex_lock(&mutexLock);
    FILE *fileFD = fopen(dataFile,"a+");

    //Check if we have got a IOCSEEKTO
    if (strncmp(outputBuffer, ioctl_string, strlen(ioctl_string)) == 0)
    {
        //IOCTL call with params
        struct aesd_seekto seek_params;
        sscanf(outputBuffer, "AESDCHAR_IOCSEEKTO:%d,%d", &seek_params.write_cmd, &seek_params.write_cmd_offset);
        
        //Get the file number to call IOCTL
        int file_number = fileno(fileFD);
        ioctl(file_number, AESDCHAR_IOCSEEKTO, &seek_params);

    } else { //regular

	    //Write buffer to file
	    fwrite(outputBuffer,1, strlen(outputBuffer),fileFD);
            //The specs mention not to close the file, we'll rewind to send the entire contents back
	    rewind(fileFD);
    }

    //send back the contents of the file
    int bytes_read=0;
    sendBuffer = (char *)malloc(sizeof(char) * (1));

    //may not be very efficient but works good for our data sizes
    while((bytes_read = fread(sendBuffer, 1, 1, fileFD)) > 0)
    {
      send(params->threadFD, sendBuffer, bytes_read, 0);
    }

    //Close 
    fclose(fileFD);
    params->isDone = true;

    //Release memory
    free(outputBuffer);
    free(sendBuffer);

    //Unlock mutex
    pthread_mutex_unlock(&mutexLock);

    return params;
}

void write_pid_file() {
    int pid_fd = open(PID_FILE, O_CREAT | O_RDWR, 0644);
    if (pid_fd == -1) {
        syslog(LOG_ERR, "Failed to open the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (flock(pid_fd, LOCK_EX | LOCK_NB) == -1) {
        syslog(LOG_ERR, "Failed to lock the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());

#if USE_AESD_CHAR_DEVICE == 1
    pthread_mutex_lock(&data_mutex);
    if (write(aesd_char_fd, pid_str, strlen(pid_str)) == -1) {
        syslog(LOG_ERR, "Failed to write to /dev/aesdchar: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&data_mutex);
#else
    pthread_mutex_lock(&data_mutex);
    if (write(pid_fd, pid_str, strlen(pid_str)) == -1) {
        syslog(LOG_ERR, "Failed to write to the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&data_mutex);
#endif
}

void* connection_handler(void* client_socket_ptr) {
    int client_socket = *((int*)client_socket_ptr);
    free(client_socket_ptr);

    handle_connection(client_socket);

    return NULL;
}

void* timestamp_updater(void* arg) {
    while (1) {
        sleep(TIMESTAMP_INTERVAL);
#if USE_AESD_CHAR_DEVICE == 1
        // Timestamp printing is removed
#else
        pthread_mutex_lock(&timestamp_mutex);
        char timestamp[50];
        time_t t = time(NULL);
        if (t - last_timestamp >= TIMESTAMP_INTERVAL) {
            struct tm current_time;
            if (localtime_r(&t, &current_time) != NULL) {
                ///strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", &current_time);
                pthread_mutex_lock(&data_mutex);
                if (write(data_fd, timestamp, strlen(timestamp)) == -1) {
                    syslog(LOG_ERR, "Failed to write timestamp to %s: %s", DATA_FILE, strerror(errno));
                }
                pthread_mutex_unlock(&data_mutex);
            } else {
                syslog(LOG_ERR, "Failed to get current time: %s", strerror(errno));
            }
            last_timestamp = t;
        }
        pthread_mutex_unlock(&timestamp_mutex);
#endif
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        int pid_fd = open(PID_FILE, O_RDONLY);
        if (pid_fd != -1) {
            char pid_str[16];
            ssize_t n = read(pid_fd, pid_str, sizeof(pid_str));
            if (n > 0) {
                pid_t existing_pid = atoi(pid_str);
                if (kill(existing_pid, 0) == 0) {
                    syslog(LOG_ERR, "Daemon is already running with PID %d.", existing_pid);
                    close(pid_fd);
                    exit(EXIT_FAILURE);
                }
            }
            close(pid_fd);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return EXIT_FAILURE;
        } else if (pid > 0) {
            return EXIT_SUCCESS;
        }

        setsid();
        umask(0);
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        write_pid_file();
    }

    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    pthread_t timestamp_thread;

    if (pthread_mutex_init(&data_mutex, NULL) != 0) {
        syslog(LOG_ERR, "Failed to initialize the data_mutex");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&timestamp_mutex, NULL) != 0) {
        syslog(LOG_ERR, "Failed to initialize the timestamp_mutex");
        return EXIT_FAILURE;
    }

    last_timestamp = time(NULL);

    if (pthread_create(&timestamp_thread, NULL, timestamp_updater, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create the timestamp updater thread");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind to port %d: %s", PORT, strerror(errno));
        return EXIT_FAILURE;
    }

    if (listen(listen_socket, 5) == -1) {
        syslog(LOG_ERR, "Failed to listen on the socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }

#if USE_AESD_CHAR_DEVICE == 1
    if ((aesd_char_fd = open("/dev/aesdchar", O_RDWR)) == -1) {
        syslog(LOG_ERR, "Failed to open /dev/aesdchar: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
#endif

    data_fd = open(DATA_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    while (1) {
        int client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == -1) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        int* client_socket_ptr = (int*)malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            syslog(LOG_ERR, "Failed to allocate memory for client_socket_ptr");
            close(client_socket);
            continue;
        }
        *client_socket_ptr = client_socket;

        pthread_t thread;
        if (pthread_create(&thread, NULL, connection_handler, (void*)client_socket_ptr) != 0) {
            syslog(LOG_ERR, "Failed to create a thread");
            free(client_socket_ptr);
            close(client_socket);
            continue;
        }

        pthread_detach(thread);
    }
#if USE_AESD_CHAR_DEVICE == 1
    if (aesd_char_fd != -1)
        close(aesd_char_fd);
#endif

    if (data_fd != -1)
        close(data_fd);

    if (listen_socket != -1)
        close(listen_socket);

    pthread_mutex_destroy(&data_mutex);
    pthread_mutex_destroy(&timestamp_mutex);

    return EXIT_SUCCESS;
}

