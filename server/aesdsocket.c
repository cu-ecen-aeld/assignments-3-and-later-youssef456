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
#include <sys/file.h>
#include <sys/file.h>

#define PORT 9000
#define MAX_PACKET_SIZE 4096
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define PID_FILE "/var/run/aesdsocket.pid"

int listen_socket, data_fd;

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");

        // Close and remove the PID file
        if (data_fd != -1)
            close(data_fd);
        if (listen_socket != -1)
            close(listen_socket);
        remove(DATA_FILE);
        remove(PID_FILE);

        closelog();
        exit(EXIT_SUCCESS);
    }
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
        if (write(data_fd, buffer, bytes_received) == -1) {
            syslog(LOG_ERR, "Failed to write data to %s: %s", DATA_FILE, strerror(errno));
            break;
        }

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
    if (write(pid_fd, pid_str, strlen(pid_str)) == -1) {
        syslog(LOG_ERR, "Failed to write to the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
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
        handle_connection(client_socket);
    }

    return EXIT_SUCCESS;
}

