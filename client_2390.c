/* NetMessenger - IE3010 | IT21212390 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/stat.h>

#define SERVER_PORT 8390
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096

#define MAX_FILE_SIZE (10LL * 1024 * 1024)


static volatile int running = 1;

static int send_line(int fd, const char *line) {
    if (send(fd, line, strlen(line), 0) < 0) { perror("send"); return -1; }
    if (send(fd, "\n", 1, 0) < 0) { perror("send"); return -1; }
    return 0;
}

static int receive_line(int fd, char *buffer, size_t size) {
    size_t used = 0;
    while (used < size - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) return 0;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (c == '\n') { buffer[used] = '\0'; return 1; }
        buffer[used++] = c;
    }
    buffer[used] = '\0';
    return 2;
}

static int receive_exact_to_file(int fd,
                                 FILE *file,
                                 long long filesize)
{
    char buffer[8192];
    long long remaining = filesize;

    while (remaining > 0) {

        size_t to_receive = sizeof(buffer);

        if (remaining < (long long)sizeof(buffer)) {
            to_receive = (size_t)remaining;
        }

        ssize_t received =
            recv(fd,
                 buffer,
                 to_receive,
                 0);

        if (received == 0) {
            return -1;
        }

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        size_t written =
            fwrite(buffer,
                   1,
                   (size_t)received,
                   file);

        if (written != (size_t)received) {
            return -1;
        }

        remaining -= (long long)received;
    }

    return 0;
}

static void *receiver_thread(void *arg)
{
    int fd = *(int *)arg;
    char response[BUFFER_SIZE];

    while (running) {

        int r = receive_line(fd,
                             response,
                             sizeof(response));

        if (r == 0) {
            printf("\nServer disconnected.\n");
            running = 0;
            break;
        }

        if (r < 0) {
            perror("recv");
            running = 0;
            break;
        }

        if (r == 2) {
            printf("\nServer response too long.\n");
            running = 0;
            break;
        }


        /*
         * =====================================================
         * FILEDATA
         *
         * Format:
         *
         * FILEDATA <sender> <filename> <filesize>
         *
         * The next <filesize> bytes are raw file data.
         * =====================================================
         */
        if (strncmp(response, "FILEDATA ", 9) == 0) {

            char header_copy[BUFFER_SIZE];

            strncpy(header_copy,
                    response,
                    sizeof(header_copy) - 1);

            header_copy[sizeof(header_copy) - 1] = '\0';


            /*
             * Skip "FILEDATA "
             */
            char *sender = header_copy + 9;

            /*
             * Find filename.
             */
            char *filename = strchr(sender, ' ');

            if (filename == NULL) {
                printf("\nInvalid FILEDATA header.\n");
                running = 0;
                break;
            }

            *filename = '\0';
            filename++;


            /*
             * Find filesize.
             */
            char *filesize_str = strchr(filename, ' ');

            if (filesize_str == NULL) {
                printf("\nInvalid FILEDATA header.\n");
                running = 0;
                break;
            }

            *filesize_str = '\0';
            filesize_str++;


            /*
             * Convert filesize.
             */
            char *endptr;
            errno = 0;

            long long filesize =
                strtoll(filesize_str,
                        &endptr,
                        10);

            if (errno != 0 ||
                *endptr != '\0' ||
                filesize < 0) {

                printf("\nInvalid FILEDATA filesize.\n");
                running = 0;
                break;
            }


            /*
             * Prevent unsafe filename.
             */
            if (strchr(filename, '/') != NULL ||
                strchr(filename, '\\') != NULL ||
                strcmp(filename, ".") == 0 ||
                strcmp(filename, "..") == 0) {

                printf("\nInvalid received filename.\n");
                running = 0;
                break;
            }


            /*
             * Create local filename.
             *
             * Example:
             *
             * received_test.txt
             */
            char output_filename[BUFFER_SIZE];

            int output_len =
                snprintf(output_filename,
                         sizeof(output_filename),
                         "received_%s",
                         filename);

            if (output_len < 0 ||
                (size_t)output_len >= sizeof(output_filename)) {

                printf("\nReceived filename too long.\n");
                running = 0;
                break;
            }


            /*
             * Open destination file.
             */
            FILE *file =
                fopen(output_filename, "wb");

            if (file == NULL) {
                perror("fopen");
                running = 0;
                break;
            }


            printf("\nReceiving file from %s: "
                   "%s (%lld bytes)\n",
                   sender,
                   filename,
                   filesize);

            fflush(stdout);


            /*
             * Receive EXACTLY <filesize> bytes.
             */
            if (receive_exact_to_file(fd,
                                      file,
                                      filesize) != 0) {

                fclose(file);
                remove(output_filename);

                printf("File reception failed.\n");

                running = 0;
                break;
            }

            fclose(file);


            printf("File received successfully: "
                   "%s (%lld bytes)\n",
                   output_filename,
                   filesize);

            printf("> ");
            fflush(stdout);

            continue;
        }


        /*
         * =====================================================
         * NORMAL SERVER MESSAGE
         * =====================================================
         */

        printf("\nServer: %s\n> ", response);
        fflush(stdout);


        /*
         * Server is closing connection.
         */
        if (strncmp(response, "OK BYE", 6) == 0) {
            running = 0;
            break;
        }
    }

    return NULL;
}

static int send_all(int fd, const void *buffer, size_t length)
{
    const char *data = buffer;
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (n == 0) {
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    char input[BUFFER_SIZE];
    char username[32];
    char response[BUFFER_SIZE];

    if (fd < 0) { perror("socket"); return EXIT_FAILURE; }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton"); close(fd); return EXIT_FAILURE;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return EXIT_FAILURE;
    }

        printf("Connected to NetMessenger server %s:%d\nEnter username: ",SERVER_IP, SERVER_PORT);
    fflush(stdout);

    if (!fgets(username, sizeof(username), stdin)) {
        close(fd);
        return EXIT_FAILURE;
    }

    /*
     * If there is no newline, the username was longer than
     * the 31-character username buffer.
     */
    if (strchr(username, '\n') == NULL &&
        strchr(username, '\r') == NULL) {
        printf("Username must contain 1-31 characters.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    username[strcspn(username, "\r\n")] = '\0';

    if (strlen(username) == 0) {
        printf("Username must contain 1-31 characters.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    char command[BUFFER_SIZE];

    snprintf(command, sizeof(command),"REGISTER %s", username);

    if (send_line(fd, command) < 0) { close(fd); return EXIT_FAILURE; }

    int r = receive_line(fd, response, sizeof(response));
    if (r != 1) { printf("Failed to receive registration response.\n"); close(fd); return EXIT_FAILURE; }
    printf("Server: %s\n", response);
    if (strncmp(response, "OK REGISTERED ", 14) != 0) { close(fd); return EXIT_FAILURE; }

    pthread_t receiver;
    if (pthread_create(&receiver, NULL, receiver_thread, &fd) != 0) {
        perror("pthread_create"); close(fd); return EXIT_FAILURE;
    }

    printf("\nRegistration successful.\n");
    printf("Commands: LIST | BCAST <message> | QUIT\n\n");

    while (running) {

        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\r\n")] = '\0';

        if (!strlen(input)) {
            continue;
        }


        /*
         * =========================================================
         * SENDFILE
         *
         * User enters:
         *
         * SENDFILE <target> <filename>
         *
         * The client calculates the file size and sends:
         *
         * SENDFILE <target> <filename> <filesize>\n
         *
         * followed immediately by the raw file bytes.
         * =========================================================
         */
        if (strncmp(input, "SENDFILE ", 9) == 0) {

            char command_copy[BUFFER_SIZE];

            strncpy(command_copy,
                    input,
                    sizeof(command_copy) - 1);

            command_copy[sizeof(command_copy) - 1] = '\0';


            /*
             * Remove "SENDFILE "
             */
            char *target = command_copy + 9;


            /*
             * Find the space between target and filename.
             */
            char *filename = strchr(target, ' ');

            if (filename == NULL) {
                printf("Usage: SENDFILE <target> <filename>\n");
                continue;
            }


            /*
             * Split into:
             *
             * target   = Chamod
             * filename = test.txt
             */
            *filename = '\0';
            filename++;


            if (*target == '\0' || *filename == '\0') {
                printf("Usage: SENDFILE <target> <filename>\n");
                continue;
            }


            /*
             * Open the file.
             */
            FILE *file = fopen(filename, "rb");

            if (file == NULL) {
                perror("fopen");
                continue;
            }


            /*
             * Find file size.
             */
            if (fseek(file, 0, SEEK_END) != 0) {
                perror("fseek");
                fclose(file);
                continue;
            }

            long file_size = ftell(file);

            if (file_size < 0) {
                perror("ftell");
                fclose(file);
                continue;
            }

            rewind(file);


            /*
             * Check the maximum file size.
             */
            if ((long long)file_size > MAX_FILE_SIZE) {
                printf("File too large. Maximum size is 10 MB.\n");
                fclose(file);
                continue;
            }


            /*
             * Prevent path traversal / invalid filenames.
             */
            if (strchr(filename, '/') != NULL ||
                strchr(filename, '\\') != NULL ||
                strcmp(filename, ".") == 0 ||
                strcmp(filename, "..") == 0) {

                printf("Invalid filename.\n");
                fclose(file);
                continue;
            }


            /*
             * Build the SENDFILE header.
             */
            char header[BUFFER_SIZE];

            int header_len = snprintf(
                header,
                sizeof(header),
                "SENDFILE %s %s %ld\n",
                target,
                filename,
                file_size
            );

            if (header_len < 0 ||
                (size_t)header_len >= sizeof(header)) {

                printf("SENDFILE command too long.\n");
                fclose(file);
                continue;
            }


            /*
             * Send the command/header.
             */
            if (send_all(fd,
                         header,
                         (size_t)header_len) != 0) {

                perror("send");
                fclose(file);
                running = 0;
                break;
            }


            /*
             * Send the actual file bytes.
             *
             * These are RAW bytes.
             *
             * We do NOT use send_line() here.
             */
            char file_buffer[8192];
            size_t bytes_sent = 0;

            while (bytes_sent < (size_t)file_size) {

                size_t remaining =
                    (size_t)file_size - bytes_sent;

                size_t to_read =
                    remaining < sizeof(file_buffer)
                    ? remaining
                    : sizeof(file_buffer);


                size_t bytes_read =
                    fread(file_buffer,
                          1,
                          to_read,
                          file);

                if (bytes_read == 0) {

                    if (ferror(file)) {
                        perror("fread");
                    } else {
                        printf("Unexpected end of file.\n");
                    }

                    fclose(file);
                    running = 0;
                    break;
                }


                /*
                 * Send exactly the bytes read.
                 */
                if (send_all(fd,
                             file_buffer,
                             bytes_read) != 0) {

                    perror("send");
                    fclose(file);
                    running = 0;
                    break;
                }

                bytes_sent += bytes_read;
            }

            fclose(file);


            if (!running) {
                break;
            }


            printf("[INFO] File sent: %s (%ld bytes)\n",
                   filename,
                   file_size);

            /*
             * Do NOT call send_line() here.
             *
             * The server will send:
             *
             * OK FILE_RECEIVED <filename> NID:2123
             *
             * and receiver_thread() will display it.
             */

            continue;
        }


        /*
         * =========================================================
         * NORMAL COMMANDS
         * =========================================================
         *
         * LIST
         * BCAST
         * PMSG
         * JOIN
         * LEAVE
         * ROOMS
         * RMSG
         * etc.
         *
         * These are normal line-based commands.
         */
        if (send_line(fd, input) < 0) {
            running = 0;
            break;
        }


        /*
         * =========================================================
         * QUIT
         * =========================================================
         */
        if (strcmp(input, "QUIT") == 0) {

            /*
             * Receiver thread will display:
             *
             * OK BYE NID:2123
             */
            break;
        }
    }

    running = 0;
    shutdown(fd, SHUT_RDWR);
    pthread_join(receiver, NULL);
    close(fd);
    printf("Client closed.\n");
    return EXIT_SUCCESS;
}
