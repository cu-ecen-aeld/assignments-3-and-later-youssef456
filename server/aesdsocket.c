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
#include <time.h>

#define PORT 9000
#define MAX_PACKET_SIZE 4096
#define DATA_FILE "/dev/aesdchar"
#define PID_FILE "/var/run/aesdsocket.pid"
#define TIMESTAMP_INTERVAL 10

int listen_socket = -1, data_fd = -1;
pthread_mutex_t data_mutex;
pthread_mutex_t timestamp_mutex;
time_t last_timestamp;

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");

        pthread_mutex_lock(&data_mutex);
        if (data_fd != -1)
            close(data_fd);
        if (listen_socket != -1)
            close(listen_socket);
        remove(DATA_FILE);
        remove(PID_FILE);
        pthread_mutex_unlock(&data_mutex);

        closelog();
        exit(EXIT_SUCCESS);
    }
}

void append_timestamp() {
    char timestamp[50];
    struct tm current_time;
    time_t t = time(NULL);

    if (localtime_r(&t, &current_time) == NULL) {
        syslog(LOG_ERR, "Failed to get current time: %s", strerror(errno));
        return;
    }

    if (strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", &current_time) == 0) {
        syslog(LOG_ERR, "Failed to format timestamp: %s", strerror(errno));
        return;
    }

    pthread_mutex_lock(&timestamp_mutex);
    if (write(data_fd, timestamp, strlen(timestamp)) == -1) {
        syslog(LOG_ERR, "Failed to write timestamp to %s: %s", DATA_FILE, strerror(errno));
    }
    pthread_mutex_unlock(&timestamp_mutex);

    last_timestamp = t;
}

void handle_connection(int client_socket) {
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
    } else {
        syslog(LOG_ERR, "Failed to get client IP address");
    }

    char buffer[MAX_PACKET_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        pthread_mutex_lock(&data_mutex);
        
        int append_flags = O_WRONLY | O_APPEND | O_CREAT;
        if (data_fd == -1) {
            // If the file descriptor is not yet open, open it in append mode
            data_fd = open(DATA_FILE, append_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (data_fd == -1) {
                syslog(LOG_ERR, "Failed to open %s: %s", DATA_FILE, strerror(errno));
                pthread_mutex_unlock(&data_mutex);
                break;
            }
        }
        
        
        if (write(data_fd, buffer, bytes_received) == -1) {
            syslog(LOG_ERR, "Failed to write data to %s: %s", DATA_FILE, strerror(errno));
            pthread_mutex_unlock(&data_mutex);
            break;
        }
        pthread_mutex_unlock(&data_mutex);

        for (int i = 0; i < bytes_received; i++) {
            if (buffer[i] == '\n') {
                lseek(data_fd, 0, SEEK_SET);
                char read_buffer[MAX_PACKET_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(data_fd, read_buffer, sizeof(read_buffer))) > 0) {
                    if (send(client_socket, read_buffer, bytes_read, 0) == -1) {
                        syslog(LOG_ERR, "Failed to send data to the client: %s", strerror(errno));
                        break;
                    }
                }
                // Append a newline after sending the contents of the file
                send(client_socket, "\n", 1, 0);
            }
        }
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    close(client_socket);
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

    pthread_mutex_lock(&data_mutex);
    if (write(pid_fd, pid_str, strlen(pid_str)) == -1) {
        syslog(LOG_ERR, "Failed to write to the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&data_mutex);
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
        append_timestamp();
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

    pthread_mutex_destroy(&data_mutex);
    pthread_mutex_destroy(&timestamp_mutex);

    return EXIT_SUCCESS;
}

