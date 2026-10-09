/*
 * NetMessenger - IE3010
 * Registration Number: IT21212390
 * Personalized Port: 8390
 * NID: 2123
 *
 * Step 1:
 * - TCP server
 * - Multiple client connections using pthreads
 * - REGISTER command
 * - Unique username checking
 * - QUIT command
 * - Basic disconnect cleanup
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8390
#define NID "NID:2123"
#define MAX_CLIENTS 50
#define MAX_USERNAME 32
#define BUFFER_SIZE 4096

typedef struct {
    int socket_fd;
    int active;
    char username[MAX_USERNAME];
} Client;

static Client clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void send_response(int fd, const char *message)
{
    char response[BUFFER_SIZE];
    int n = snprintf(response, sizeof(response), "%s %s\n", message, NID);

    if (n < 0 || (size_t)n >= sizeof(response)) {
        return;
    }

    send(fd, response, (size_t)n, 0);
}

static int valid_username(const char *username)
{
    size_t len = strlen(username);

    if (len == 0 || len >= MAX_USERNAME) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        if (username[i] == ' ' || username[i] == '\t' ||
            username[i] == '\r' || username[i] == '\n') {
            return 0;
        }
    }

    return 1;
}

static int find_client_by_username(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active &&
            strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }

    return -1;
}

static int add_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket_fd = fd;
            clients[i].active = 1;
            clients[i].username[0] = '\0';
            return i;
        }
    }

    return -1;
}

static void remove_client(int index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        return;
    }

    clients[index].active = 0;
    clients[index].socket_fd = -1;
    clients[index].username[0] = '\0';
}

static int read_line(int fd, char *buffer, size_t buffer_size)
{
    size_t used = 0;

    while (used < buffer_size - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0) {
            return 0; /* Client disconnected */
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (c == '\n') {
            buffer[used] = '\0';
            return 1;
        }

        buffer[used++] = c;
    }

    buffer[used] = '\0';
    return 2; /* Line too long */
}

static void *client_handler(void *arg)
{
    int client_index = *(int *)arg;
    free(arg);

    int fd = clients[client_index].socket_fd;
    char line[BUFFER_SIZE];
    int registered = 0;

    printf("[INFO] Client connected: socket=%d\n", fd);

    while (1) {
        int result = read_line(fd, line, sizeof(line));

        if (result == 0) {
            printf("[INFO] Client disconnected: socket=%d\n", fd);
            break;
        }

        if (result < 0) {
            perror("[ERROR] recv");
            break;
        }

        if (result == 2) {
            send_response(fd, "ERR 005 COMMAND_TOO_LONG");
            continue;
        }

        /* Remove optional CR if a client sends CRLF. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        if (!registered) {
            if (strncmp(line, "REGISTER ", 9) != 0) {
                send_response(fd, "ERR 005 REGISTER_REQUIRED");
                continue;
            }

            const char *username = line + 9;

            if (!valid_username(username)) {
                send_response(fd, "ERR 005 INVALID_USERNAME");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            if (find_client_by_username(username) != -1) {
                pthread_mutex_unlock(&clients_mutex);
                send_response(fd, "ERR 001 USERNAME_TAKEN");
                continue;
            }

            strncpy(clients[client_index].username,
                    username,
                    MAX_USERNAME - 1);
            clients[client_index].username[MAX_USERNAME - 1] = '\0';
            registered = 1;

            pthread_mutex_unlock(&clients_mutex);

            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response),
                     "OK REGISTERED %s", username);
            send_response(fd, response);

            printf("[INFO] Registered username='%s' socket=%d\n",
                   username, fd);
            continue;
        }

        if (strcmp(line, "QUIT") == 0) {
            send_response(fd, "OK BYE");
            break;
        }

        /* Other commands will be implemented in later steps. */
        send_response(fd, "ERR 005 UNKNOWN_COMMAND");
    }

    pthread_mutex_lock(&clients_mutex);
    remove_client(client_index);
    pthread_mutex_unlock(&clients_mutex);

    close(fd);
    return NULL;
}

int main(void)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in server_addr;

    signal(SIGPIPE, SIG_IGN);

    memset(clients, 0, sizeof(clients));

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("=============================================\n");
    printf("NetMessenger Server\n");
    printf("Registration : IT21212390\n");
    printf("Listening    : TCP port %d\n", PORT);
    printf("Node ID      : %s\n", NID);
    printf("=============================================\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int client_index = add_client(client_fd);
        pthread_mutex_unlock(&clients_mutex);

        if (client_index == -1) {
            send_response(client_fd, "ERR 006 SERVER_FULL");
            close(client_fd);
            continue;
        }

        int *index_arg = malloc(sizeof(int));
        if (index_arg == NULL) {
            perror("malloc");
            pthread_mutex_lock(&clients_mutex);
            remove_client(client_index);
            pthread_mutex_unlock(&clients_mutex);
            close(client_fd);
            continue;
        }

        *index_arg = client_index;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, index_arg) != 0) {
            perror("pthread_create");
            free(index_arg);

            pthread_mutex_lock(&clients_mutex);
            remove_client(client_index);
            pthread_mutex_unlock(&clients_mutex);

            close(client_fd);
            continue;
        }

        pthread_detach(thread);

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  ip, sizeof(ip));

        printf("[INFO] Accepted %s:%d socket=%d\n",
               ip, ntohs(client_addr.sin_port), client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
