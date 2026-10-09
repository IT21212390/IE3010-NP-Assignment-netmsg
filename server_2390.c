/* NetMessenger - IE3010 | IT21212390*/
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>

#include <time.h>
#include <stdarg.h>

#define PORT 8390
#define NID "NID:2123"
#define MAX_CLIENTS 50
#define MAX_USERNAME 32
#define BUFFER_SIZE 4096

#define MAX_ROOMS 50
#define MAX_ROOM_MEMBERS 50
#define MAX_ROOM_NAME 32

#define MAX_FILE_SIZE (10LL * 1024 * 1024)

#define LOG_FILE "netmsg_IT21212390.log"

static void log_event(const char *format, ...)
{
    FILE *log = fopen(LOG_FILE, "a");

    if (log == NULL) {
        perror("fopen log");
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timestamp[32];

    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log, "[%s] ", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(log, format, args);
    va_end(args);

    fprintf(log, "\n");

    fclose(log);
}

typedef struct {
    int active;
    char name[MAX_ROOM_NAME];
    int members[MAX_ROOM_MEMBERS];
} Room;

static Room rooms[MAX_ROOMS];

typedef struct { int socket_fd; int active; int registered; char username[MAX_USERNAME]; } Client;
static Client clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void send_response(int fd, const char *message) {
    char response[BUFFER_SIZE];
    int n = snprintf(response, sizeof(response), "%s %s\n", message, NID);
    if (n > 0 && (size_t)n < sizeof(response)) send(fd, response, (size_t)n, 0);
}

static int send_line(int fd, const char *message) {
    size_t len = strlen(message);
    if (send(fd, message, len, 0) < 0) return -1;
    if (send(fd, "\n", 1, 0) < 0) return -1;
    return 0;
}

static int send_all(int fd, const void *buffer, size_t length)
{
    const char *data = buffer;
    size_t sent = 0;

    while (sent < length) {

        ssize_t n = send(fd,
                         data + sent,
                         length - sent,
                         0);

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

static int valid_username(const char *u) {
    size_t len = strlen(u);
    if (len == 0 || len >= MAX_USERNAME) return 0;
    for (size_t i = 0; i < len; i++)
        if (u[i] == ' ' || u[i] == '\t' || u[i] == '\r' || u[i] == '\n') return 0;
    return 1;
}

static int find_client_by_username(const char *u) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && clients[i].registered && strcmp(clients[i].username, u) == 0) return i;
    return -1;
}

static int add_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket_fd = fd; clients[i].active = 1; clients[i].registered = 0;
            clients[i].username[0] = '\0'; return i;
        }
    }
    return -1;
}

static void remove_client(int index) {
    clients[index].active = 0; clients[index].registered = 0;
    clients[index].socket_fd = -1; clients[index].username[0] = '\0';
}

static void broadcast_presence(const char *username, const char *event) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "MSG PRESENCE %s %s", event, username);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && clients[i].registered) send_line(clients[i].socket_fd, message);
}

static void send_users(int fd) {
    char response[BUFFER_SIZE];
    size_t used = (size_t)snprintf(response, sizeof(response), "OK USERS ");
    int first = 1;
    for (int i = 0; i < MAX_CLIENTS && used < sizeof(response); i++) {
        if (clients[i].active && clients[i].registered) {
            int n = snprintf(response + used, sizeof(response) - used, "%s%s", first ? "" : ",", clients[i].username);
            if (n < 0 || (size_t)n >= sizeof(response) - used) break;
            used += (size_t)n; first = 0;
        }
    }
    send_response(fd, response);
}

static int read_line(int fd, char *buffer, size_t size) {
    size_t used = 0;
    while (used < size - 1) {
        char c; ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) return 0;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (c == '\n') { buffer[used] = '\0'; return 1; }
        buffer[used++] = c;
    }
    buffer[used] = '\0'; return 2;
}

static int find_room(const char *room_name)
{
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active &&
            strcmp(rooms[i].name, room_name) == 0) {
            return i;
        }
    }

    return -1;
}

static int create_room(const char *room_name)
{
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            rooms[i].active = 1;

            strncpy(rooms[i].name,
                    room_name,
                    MAX_ROOM_NAME - 1);

            rooms[i].name[MAX_ROOM_NAME - 1] = '\0';

            for (int j = 0; j < MAX_ROOM_MEMBERS; j++) {
                rooms[i].members[j] = -1;
            }

            return i;
        }
    }

    return -1;
}

static int is_room_member(int room_index, int client_index)
{
    for (int i = 0; i < MAX_ROOM_MEMBERS; i++) {
        if (rooms[room_index].members[i] == client_index) {
            return 1;
        }
    }

    return 0;
}

static int add_room_member(int room_index, int client_index)
{
    if (is_room_member(room_index, client_index)) {
        return 1;
    }

    for (int i = 0; i < MAX_ROOM_MEMBERS; i++) {
        if (rooms[room_index].members[i] == -1) {
            rooms[room_index].members[i] = client_index;
            return 1;
        }
    }

    return 0;
}

static void remove_room_member(int room_index, int client_index)
{
    for (int i = 0; i < MAX_ROOM_MEMBERS; i++) {
        if (rooms[room_index].members[i] == client_index) {
            rooms[room_index].members[i] = -1;
            return;
        }
    }
}

static int receive_exact_bytes(int fd, FILE *file, long long filesize)
{
    char buffer[8192];
    long long remaining = filesize;

    while (remaining > 0) {
        size_t to_receive = sizeof(buffer);

        if (remaining < (long long)sizeof(buffer)) {
            to_receive = (size_t)remaining;
        }

        ssize_t received = recv(fd, buffer, to_receive, 0);

        if (received == 0) {
            return -1;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        size_t written = fwrite(buffer, 1, (size_t)received, file);

        if (written != (size_t)received) {
            return -1;
        }

        remaining -= received;
    }

    return 0;
}

static int discard_exact_bytes(int fd, long long filesize)
{
    char buffer[8192];
    long long remaining = filesize;

    while (remaining > 0) {

        size_t to_receive = sizeof(buffer);

        if (remaining < (long long)sizeof(buffer)) {
            to_receive = (size_t)remaining;
        }

        ssize_t received = recv(fd, buffer, to_receive, 0);

        if (received == 0) {
            return -1;
        }

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        remaining -= (long long)received;
    }

    return 0;
}

static int send_file_to_client(int fd, const char *filepath, const char *sender,const char *filename, long long filesize)
{
    char header[BUFFER_SIZE];

    int header_len = snprintf(
        header,
        sizeof(header),
        "FILEDATA %s %s %lld\n",
        sender,
        filename,
        filesize
    );

    if (header_len < 0 ||
        (size_t)header_len >= sizeof(header)) {
        return -1;
    }

    /*
     * Send FILEDATA header.
     *
     * Example:
     *
     * FILEDATA Pabasara test.txt 46
     */
    if (send_all(fd,
                 header,
                 (size_t)header_len) != 0) {
        return -1;
    }

    FILE *file = fopen(filepath, "rb");

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    char buffer[8192];
    long long remaining = filesize;

    while (remaining > 0) {

        size_t to_read = sizeof(buffer);

        if (remaining < (long long)sizeof(buffer)) {
            to_read = (size_t)remaining;
        }

        size_t bytes_read =
            fread(buffer, 1, to_read, file);

        if (bytes_read == 0) {
            fclose(file);
            return -1;
        }

        if (send_all(fd,
                     buffer,
                     bytes_read) != 0) {
            fclose(file);
            return -1;
        }

        remaining -= (long long)bytes_read;
    }

    fclose(file);

    return 0;
}


static void *client_handler(void *arg) {


    int client_index = *(int *)arg;
    free(arg);

    int idx = client_index;
    int fd = clients[idx].socket_fd, registered = 0;
    char line[BUFFER_SIZE], username[MAX_USERNAME] = "";
    printf("[INFO] Client connected: socket=%d\n", fd);

    log_event("[INFO] Client connected: socket=%d\n", fd);

    while (1) {
        int r = read_line(fd, line, sizeof(line));
        if (r == 0) {
            printf("[INFO] Client disconnected: socket=%d\n", fd); break;
            log_event("[INFO] Client disconnected: socket=%d\n", fd); break;

        }
        if (r < 0) { perror("recv"); break; }
        if (r == 2) {
            send_response(fd, "ERR 005 COMMAND_TOO_LONG");
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }
        size_t len = strlen(line);
        if (len && line[len-1] == '\r') line[len-1] = '\0';

        if (!registered) {
            if (strncmp(line, "REGISTER ", 9) != 0) { send_response(fd, "ERR 005 REGISTER_REQUIRED"); continue; }
            const char *u = line + 9;
            if (!valid_username(u)) { send_response(fd, "ERR 005 INVALID_USERNAME"); log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd); continue; }
            pthread_mutex_lock(&clients_mutex);
            if (find_client_by_username(u) != -1) { pthread_mutex_unlock(&clients_mutex); send_response(fd, "ERR 001 USERNAME_TAKEN"); continue; }
            strncpy(clients[idx].username, u, MAX_USERNAME - 1); clients[idx].username[MAX_USERNAME - 1] = '\0';
            clients[idx].registered = 1; strncpy(username, u, MAX_USERNAME - 1); username[MAX_USERNAME - 1] = '\0'; registered = 1;
            char response[BUFFER_SIZE]; snprintf(response, sizeof(response), "OK REGISTERED %s", username); send_response(fd, response); log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            broadcast_presence(username, "JOINED");
            pthread_mutex_unlock(&clients_mutex);
            printf("[INFO] Registered username='%s' socket=%d\n", username, fd);
            log_event("[INFO] Registered username='%s' socket=%d\n", username, fd);
            continue;
        }

        if (strcmp(line, "LIST") == 0) {
            pthread_mutex_lock(&clients_mutex); send_users(fd); pthread_mutex_unlock(&clients_mutex); continue;
        }

        if (strncmp(line, "PMSG ", 5) == 0) {
            char *target = line + 5;
            char *message = strchr(target, ' ');

            if (message == NULL || *(message + 1) == '\0') {
                send_response(fd, "ERR 005 INVALID_PMSG");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            *message = '\0';
            message++;

            pthread_mutex_lock(&clients_mutex);

        int target_index = find_client_by_username(target);

        pthread_mutex_unlock(&clients_mutex);

        if (target_index == -1) {
            send_response(fd, "ERR 002 USER_NOT_FOUND");
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }

            char forwarded[BUFFER_SIZE];

            snprintf(forwarded, sizeof(forwarded),
                     "MSG PRIV %s %s",
                     username, message);

            send_line(clients[target_index].socket_fd, forwarded);

            send_response(fd, "OK SENT");
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }


        if (strncmp(line, "BCAST ", 6) == 0) {
            const char *message = line + 6;
            if (*message == '\0') { send_response(fd, "ERR 005 EMPTY_MESSAGE");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }
            char forwarded[BUFFER_SIZE]; snprintf(forwarded, sizeof(forwarded), "MSG BCAST %s %s", username, message);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++)
                if (clients[i].active && clients[i].registered && i != idx) send_line(clients[i].socket_fd, forwarded);
            pthread_mutex_unlock(&clients_mutex);
            send_response(fd, "OK SENT");
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;

        }

        if (strncmp(line, "JOIN ", 5) == 0) {

            const char *room_name = line + 5;

            pthread_mutex_lock(&state_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1) {
                room_index = create_room(room_name);
            }

            if (room_index == -1) {
                pthread_mutex_unlock(&state_mutex);
                send_response(fd, "ERR 006 SERVER_FULL");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            if (!add_room_member(room_index, client_index)) {
                pthread_mutex_unlock(&state_mutex);
                send_response(fd, "ERR 006 ROOM_FULL");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            pthread_mutex_unlock(&state_mutex);

            char response[BUFFER_SIZE];

            snprintf(response, sizeof(response), "OK JOINED %s", room_name);

            send_response(fd, response);
            continue;
        }

        if (strncmp(line, "LEAVE ", 6) == 0) {

            const char *room_name = line + 6;

            pthread_mutex_lock(&state_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1) {
                pthread_mutex_unlock(&state_mutex);
                send_response(fd, "ERR 003 ROOM_NOT_FOUND");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            remove_room_member(room_index, client_index);

            pthread_mutex_unlock(&state_mutex);

            char response[BUFFER_SIZE];

            snprintf(response,
                     sizeof(response),
                     "OK LEFT %s",
                     room_name);

            send_response(fd, response);
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }

        if (strcmp(line, "ROOMS") == 0) {

            char response[BUFFER_SIZE];

            strcpy(response, "OK ROOMS ");

            pthread_mutex_lock(&state_mutex);

            int first = 1;

            for (int i = 0; i < MAX_ROOMS; i++) {

                if (rooms[i].active) {

                    if (!first) {
                        strcat(response, ",");
                    }

                    strcat(response, rooms[i].name);
                    first = 0;
                }
            }

            pthread_mutex_unlock(&state_mutex);

            send_response(fd, response);
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }

        if (strncmp(line, "RMSG ", 5) == 0) {

            char command_copy[BUFFER_SIZE];

            strncpy(command_copy,
                    line,
                    sizeof(command_copy) - 1);

            command_copy[sizeof(command_copy) - 1] = '\0';

            char *room_name = command_copy + 5;
            char *message = strchr(room_name, ' ');

            if (message == NULL || *(message + 1) == '\0') {
                send_response(fd, "ERR 005 INVALID_RMSG");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            *message = '\0';
            message++;

            pthread_mutex_lock(&state_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1) {
                pthread_mutex_unlock(&state_mutex);
                send_response(fd, "ERR 003 ROOM_NOT_FOUND");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            if (!is_room_member(room_index, client_index)) {
                pthread_mutex_unlock(&state_mutex);
                send_response(fd, "ERR 003 NOT_ROOM_MEMBER");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            char output[BUFFER_SIZE];

            snprintf(output,
                     sizeof(output),
                     "MSG ROOM %s %s %s",
                     room_name,
                     username,
                     message);

            for (int i = 0; i < MAX_ROOM_MEMBERS; i++) {

                int member = rooms[room_index].members[i];

                if (member >= 0 &&
                    clients[member].active &&
                    clients[member].registered) {

                    send_line(clients[member].socket_fd,
                              output);
                }
            }

            pthread_mutex_unlock(&state_mutex);

            send_response(fd, "OK SENT");
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
            continue;
        }

                if (strncmp(line, "SENDFILE ", 9) == 0) {

            char command_copy[BUFFER_SIZE];

            strncpy(command_copy, line, sizeof(command_copy) - 1);
            command_copy[sizeof(command_copy) - 1] = '\0';

            char *target = command_copy + 9;
            char *filename = strchr(target, ' ');

            if (filename == NULL) {
                send_response(fd, "ERR 005 INVALID_SENDFILE");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            *filename = '\0';
            filename++;

            char *filesize_str = strchr(filename, ' ');

            if (filesize_str == NULL) {
                send_response(fd, "ERR 005 INVALID_SENDFILE");
                continue;
            }

            *filesize_str = '\0';
            filesize_str++;

            if (*target == '\0' || *filename == '\0' || *filesize_str == '\0') {
                send_response(fd, "ERR 005 INVALID_SENDFILE");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            char *endptr;
            errno = 0;

            long long filesize = strtoll(filesize_str, &endptr, 10);

            if (errno != 0 || *endptr != '\0' || filesize < 0) {
                send_response(fd, "ERR 005 INVALID_FILESIZE");
                continue;
            }

            if (filesize > MAX_FILE_SIZE) {
                send_response(fd, "ERR 004 FILE_TOO_LARGE");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                break;
            }

            /*
             * Prevent path traversal.
             */
            if (strchr(filename, '/') != NULL ||
                strchr(filename, '\\') != NULL ||
                strcmp(filename, ".") == 0 ||
                strcmp(filename, "..") == 0) {

                send_response(fd, "ERR 005 INVALID_FILENAME");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            /*
             * For Step 5, target must be an existing username.
             */
            pthread_mutex_lock(&clients_mutex);

            int target_index = find_client_by_username(target);

            pthread_mutex_unlock(&clients_mutex);

            if (target_index == -1) {


                if (discard_exact_bytes(fd, filesize) != 0) {
                    printf("[ERROR] Failed to discard file data " "for unknown target.\n");
                    log_event("[ERROR] Failed to discard file data " "for unknown target.\n");

                    break;
                }

                send_response(fd, "ERR 002 USER_NOT_FOUND");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            /*
             * Create sender storage directory.
             */
            char directory[BUFFER_SIZE];

            int dir_len = snprintf(directory,
                                   sizeof(directory),
                                   "./storage/IT21212390/%s",
                                   username);

            if (dir_len < 0 || (size_t)dir_len >= sizeof(directory)) {
                send_response(fd, "ERR 006 STORAGE_ERROR");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            if (mkdir("./storage", 0755) != 0 && errno != EEXIST) {
                send_response(fd, "ERR 006 STORAGE_ERROR");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            if (mkdir("./storage/IT21212390", 0755) != 0 && errno != EEXIST) {
                send_response(fd, "ERR 006 STORAGE_ERROR");
                continue;
            }

            if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
                send_response(fd, "ERR 006 STORAGE_ERROR");
                continue;
            }

            /*
             * Build destination file path.
             */
            char filepath[BUFFER_SIZE];

            int path_len = snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);

            if (path_len < 0 || (size_t)path_len >= sizeof(filepath)) {
                send_response(fd, "ERR 006 STORAGE_ERROR");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            FILE *file = fopen(filepath, "wb");

            if (file == NULL) {
                perror("fopen");
                send_response(fd, "ERR 006 STORAGE_ERROR");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            printf("[INFO] Receiving file: sender=%s target=%s " "filename=%s size=%lld\n",username, target, filename, filesize);

            log_event("[INFO] Receiving file: sender=%s target=%s " "filename=%s size=%lld\n",username, target, filename, filesize);

            /*
             * Receive exactly <filesize> raw bytes.
             */
            if (receive_exact_bytes(fd, file, filesize) != 0) {

                fclose(file);
                remove(filepath);

                printf("[ERROR] File transfer failed: sender=%s " "filename=%s\n", username, filename);
                log_event("[ERROR] File transfer failed: sender=%s " "filename=%s\n", username, filename);

                send_response(fd, "ERR 006 FILE_TRANSFER_FAILED");
                log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);
                continue;
            }

            fclose(file);

            printf("[INFO] File received successfully: " "%s (%lld bytes)\n", filepath, filesize);

            log_event("[INFO] File received successfully: " "%s (%lld bytes)\n", filepath, filesize);

            /*
             * Notify sender.
             */
            char response[BUFFER_SIZE];

            snprintf(response,
                     sizeof(response),
                     "OK FILE_RECEIVED %s",
                     filename);

            send_response(fd, response);
            log_event("REGISTER username=%s fd=%d", clients[client_index].username, fd);

            /*
             * Notify target that a file is available.
             */
        char notification[BUFFER_SIZE];

        snprintf(notification, sizeof(notification),"MSG FILE %s %s %lld", username, filename, filesize);

        /*
         * Get the target socket while holding the mutex.
         * Then release the mutex before sending the file.
         */
        pthread_mutex_lock(&clients_mutex);

        int target_fd = -1;

        if (clients[target_index].active &&
            clients[target_index].registered) {

            target_fd = clients[target_index].socket_fd;
        }

        pthread_mutex_unlock(&clients_mutex);


        if (target_fd != -1) {

            /*
             * First notify the receiver.
             */
            if (send_line(target_fd, notification) != 0) {
                printf("[ERROR] Failed to notify target about file.\n");
                log_event("[ERROR] Failed to notify target about file.\n");
                continue;
            }

            /*
             * Then send the FILEDATA header and
             * exactly <filesize> raw bytes.
             */
            if (send_file_to_client(target_fd,
                                    filepath,
                                    username,
                                    filename,
                                    filesize) != 0) {

                printf("[ERROR] Failed to deliver file '%s' " "to '%s'.\n", filename, target);
                log_event("[ERROR] Failed to deliver file '%s' " "to '%s'.\n", filename, target);
            } else {

                printf("[INFO] File delivered successfully: " "sender=%s target=%s filename=%s " "size=%lld\n", username, target, filename, filesize);
                log_event("[INFO] File delivered successfully: " "sender=%s target=%s filename=%s " "size=%lld\n", username, target, filename, filesize);
            }
        }

        continue;
        }

        if (strcmp(line, "QUIT") == 0) { send_response(fd, "OK BYE"); break; }
        send_response(fd, "ERR 005 UNKNOWN_COMMAND");
        log_event("REGISTER username=%s fd=%d",clients[client_index].username, fd);
    }

    pthread_mutex_lock(&clients_mutex);
    int was_registered = clients[idx].registered; char oldname[MAX_USERNAME];
    strncpy(oldname, clients[idx].username, MAX_USERNAME - 1); oldname[MAX_USERNAME - 1] = '\0';
    remove_client(idx);
    if (was_registered) broadcast_presence(oldname, "LEFT");
    pthread_mutex_unlock(&clients_mutex);
    close(fd); return NULL;
}

int main(void) {

    memset(rooms, 0, sizeof(rooms));

    for (int i = 0; i < MAX_ROOMS; i++) {
        for (int j = 0; j < MAX_ROOM_MEMBERS; j++) {
            rooms[i].members[j] = -1;
        }
    }

    int server_fd, opt = 1; struct sockaddr_in addr;
    signal(SIGPIPE, SIG_IGN); memset(clients, 0, sizeof(clients));
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { perror("setsockopt"); close(server_fd); return EXIT_FAILURE; }
    memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(server_fd); return EXIT_FAILURE; }
    if (listen(server_fd, 10) < 0) { perror("listen"); close(server_fd); return EXIT_FAILURE; }
    printf("=============================================\nNetMessenger Server - Step 2\nRegistration : IT21212390\nListening    : TCP port %d\nNode ID      : %s\nFeatures     : REGISTER LIST BCAST PRESENCE\n=============================================\n", PORT, NID);
    log_event("=============================================\nNetMessenger Server - Step 2\nRegistration : IT21212390\nListening    : TCP port %d\nNode ID      : %s\nFeatures     : REGISTER LIST BCAST PRESENCE\n=============================================\n", PORT, NID);
    while (1) {
        struct sockaddr_in client_addr; socklen_t client_len = sizeof(client_addr);
        int fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (fd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }
        pthread_mutex_lock(&clients_mutex); int idx = add_client(fd); pthread_mutex_unlock(&clients_mutex);
        if (idx < 0) { send_response(fd, "ERR 006 SERVER_FULL"); close(fd); continue; }
        int *arg = malloc(sizeof(int));
        if (!arg) { perror("malloc"); pthread_mutex_lock(&clients_mutex); remove_client(idx); pthread_mutex_unlock(&clients_mutex); close(fd); continue; }
        *arg = idx; pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, arg) != 0) { perror("pthread_create"); free(arg); pthread_mutex_lock(&clients_mutex); remove_client(idx); pthread_mutex_unlock(&clients_mutex); close(fd); continue; }
        pthread_detach(thread);
        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        printf("[INFO] Accepted %s:%d socket=%d\n", ip, ntohs(client_addr.sin_port), fd);
        log_event("[INFO] Accepted %s:%d socket=%d\n", ip, ntohs(client_addr.sin_port), fd);
    }
    close(server_fd); return EXIT_SUCCESS;
}
