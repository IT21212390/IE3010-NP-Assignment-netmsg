/*
 * NetMessenger - IE3010
 * Registration Number: IT21212390
 * Personalized Port: 8390
 * NID expected from server: NID:2123
 *
 * Step 1 client:
 * - Connect to TCP server
 * - Register username
 * - Display server responses
 * - Send QUIT
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8390
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096

static int send_line(int fd, const char *line)
{
    size_t len = strlen(line);

    if (send(fd, line, len, 0) < 0) {
        perror("send");
        return -1;
    }

    if (send(fd, "\n", 1, 0) < 0) {
        perror("send");
        return -1;
    }

    return 0;
}

static int receive_line(int fd, char *buffer, size_t buffer_size)
{
    size_t used = 0;

    while (used < buffer_size - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0) {
            return 0;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }

        if (c == '\n') {
            buffer[used] = '\0';
            return 1;
        }

        buffer[used++] = c;
    }

    buffer[used] = '\0';
    return 2;
}

int main(void)
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char username[32];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("Connected to NetMessenger server %s:%d\n",
           SERVER_IP, SERVER_PORT);

    printf("Enter username: ");
    fflush(stdout);

    if (fgets(username, sizeof(username), stdin) == NULL) {
        close(sock_fd);
        return EXIT_FAILURE;
    }

    /* Check whether the username input was too long. */
    if (strchr(username, '\n') == NULL &&
        strchr(username, '\r') == NULL) {
        printf("Username too long. Maximum is 31 characters.\n");
        close(sock_fd);
        return EXIT_FAILURE;
    }
    
    username[strcspn(username, "\r\n")] = '\0';
    
    if (strlen(username) == 0) {
        printf("Username cannot be empty.\n");
        close(sock_fd);
        return EXIT_FAILURE;
    }
    
    char command[BUFFER_SIZE];
    
    snprintf(command, sizeof(command),"REGISTER %s", username);

    if (send_line(sock_fd, command) < 0) {
        close(sock_fd);
        return EXIT_FAILURE;
    }

    int result = receive_line(sock_fd, response, sizeof(response));

    if (result == 1) {
        printf("Server: %s\n", response);
    } else if (result == 0) {
        printf("Server disconnected.\n");
        close(sock_fd);
        return EXIT_FAILURE;
    } else {
        printf("Failed to receive a complete server response.\n");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (strncmp(response, "OK REGISTERED ", 14) != 0) {
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("\nRegistration successful.\n");
    printf("Type QUIT to disconnect.\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\r\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        if (send_line(sock_fd, input) < 0) {
            break;
        }

        result = receive_line(sock_fd, response, sizeof(response));

        if (result == 0) {
            printf("Server disconnected.\n");
            break;
        }

        if (result < 0) {
            break;
        }

        if (result == 2) {
            printf("Response too long.\n");
            break;
        }

        printf("Server: %s\n", response);

        if (strcmp(input, "QUIT") == 0) {
            break;
        }
    }

    close(sock_fd);
    printf("Client closed.\n");

    return EXIT_SUCCESS;
}
