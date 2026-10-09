# NetMessenger — Multi-Client Chat and File-Sharing Platform over TCP/IP

**Module:** IE3010 — Network Programming  
**Registration Number:** IT21212390  
**Personalized Server Port:** `8390`  
**NID:** `NID:2123`  
**Language:** C | **Networking:** BSD sockets / TCP | **Concurrency:** POSIX threads

## Table of Contents
1. Project Overview
2. Objectives
3. Personalization
4. Features
5. Architecture
6. Technology Stack
7. Project Structure
8. Requirements
9. Compilation
10. Running the Application
11. Protocol Reference
12. File Transfer
13. Server Logging
14. Testing
15. Troubleshooting
16. Security Considerations and Limitations
17. Development Evidence
18. Submission Checklist
19. Author

## 1. Project Overview
NetMessenger is a TCP-based, multi-client messaging and file-sharing application implemented in C using BSD sockets and POSIX threads. A central server accepts client connections and processes commands. Users can register, list users, broadcast messages, send private messages, join chat rooms, exchange room messages, and transfer files.

TCP is a reliable, ordered byte stream; it does not preserve application message boundaries. The application therefore uses newline-terminated command/response lines. File transfer uses a header containing the file size, followed by exactly that number of raw bytes.

## 2. Objectives
- Implement TCP client-server communication using BSD sockets.
- Support multiple connected clients concurrently.
- Register users and prevent duplicate usernames.
- List registered users.
- Support broadcast and private messaging.
- Create and manage chat rooms.
- Transfer files between clients through the server.
- Handle invalid commands and common error conditions.
- Record important server events in a persistent log.
- Apply registration-based personalization requirements.

## 3. Personalization

| Item | Value |
|---|---|
| Registration number | `IT21212390` |
| Numeric portion | `21212390` |
| Last four digits | `2390` |
| Server port | `6000 + 2390 = 8390` |
| NID | `NID:2123` |
| Server source | `server_2390.c` |
| Client source | `client_2390.c` |
| Makefile | `Makefile_2390` |
| Server log | `netmsg_IT21212390.log` |
| Storage root | `./storage/IT21212390/` |
| Submission ZIP | `IE3010_IT21212390.zip` |

Normal `OK` and `ERR` responses should include the personalized NID suffix, for example:

```text
OK REGISTERED Chamod NID:2123
ERR 002 USER_NOT_FOUND NID:2123
```

## 4. Features

### User management
- Register a username and reject duplicate usernames.
- List registered users.
- Support graceful disconnect with `QUIT`.
- Presence notifications, if implemented in the current build.

### Messaging
- Broadcast messages to other registered users.
- Send private messages to a selected registered user.
- Join and leave chat rooms.
- List available rooms.
- Send messages to room members.

### File sharing
- Send a file to a target username.
- Transmit a header followed by the exact number of file bytes.
- Enforce the configured maximum file size.
- Store received files under the personalized storage directory.
- Deliver files to the target client.
- Verify file integrity using `cmp`.

### Operations
- Handle unknown users, rooms, and commands.
- Record important server events in a persistent log.
- Compile with GCC warnings enabled.

**Note:** Confirm each feature by running its test case. Do not report untested features as passing.

## 5. System Architecture

```text
+------------------+       +-----------------------------------+       +------------------------------+
| Client 1 (CLI)   |------>| TCP Server: port 8390              |------>| Persistent Log               |
+------------------+       |                                   |       | netmsg_IT21212390.log        |
                           | Main thread                       |       +------------------------------+
+------------------+       |  - Creates listening socket       |
| Client 2 (CLI)   |------>|  - Accepts connections             |       +------------------------------+
+------------------+       |  - Starts client handler threads  |------>| File Storage                 |
                           |                                   |       | ./storage/IT21212390/        |
+------------------+       | Client handler threads             |       +------------------------------+
| Client N (CLI)   |------>|  - Process commands                |
+------------------+       |  - Send/receive messages           |
                           |  - Receive file bytes               |
                           |                                   |
                           | Shared client and room state       |
                           | protected by mutexes where needed  |
                           +-----------------------------------+
```

### Main components
1. **Client application:** accepts commands, sends them to the server, and displays messages or received files.
2. **Listening socket:** binds to TCP port `8390` and accepts client connections.
3. **Client handler threads:** process commands for connected clients.
4. **Client registry:** tracks connections, usernames, and registration status.
5. **Room registry:** tracks room names and membership.
6. **File storage:** stores files received by the server.
7. **Persistent log:** records server activity and error events.

Confirm synchronization and thread behavior against the final source code before submitting the report.

## 6. Technology Stack

| Technology | Purpose |
|---|---|
| C | Application implementation |
| BSD sockets | TCP network communication |
| TCP/IP | Reliable, ordered transport |
| POSIX `pthread` | Concurrent client handling |
| POSIX file I/O | File storage and retrieval |
| GCC | Compilation |
| GNU Make | Repeatable build commands |
| Linux | Development and execution environment |
| Git | Incremental development history, if used |

## 7. Project Structure

A typical directory may contain:

```text
IE3010_IT21212390_Step7/
├── server_2390.c
├── client_2390.c
├── Makefile_2390
├── README.md
├── DESIGN_DIARY.md
├── AI_PROMPT_LOG.md
├── IMPLEMENTATION_REPORT.md
├── netmsg_IT21212390.log
├── test.txt
├── received_test.txt
└── storage/
    └── IT21212390/
        └── <sender_username>/
            └── <filename>
```

The exact contents depend on development and testing. Include or exclude binaries and test files according to the assignment's submission instructions.

## 8. Requirements
The environment should provide:
- Linux or a compatible POSIX environment.
- GCC.
- GNU Make.
- POSIX threads support.
- Standard C library and socket headers.
- A terminal for running the server and clients.

Check tools with:

```bash
gcc --version
make --version
```

## 9. Compilation
Run from the directory containing `Makefile_2390`:

```bash
make -f Makefile_2390 clean
make -f Makefile_2390
```

The build should use warning options such as `-Wall -Wextra -std=c11 -pthread`. A successful build should complete without compiler errors. Resolve compiler warnings where possible before final submission.

To remove generated binaries:

```bash
make -f Makefile_2390 clean
```

## 10. Running the Application

### Start the server
In Terminal 1:

```bash
./server_2390
```

The server should report that it is listening on port `8390`.

If startup fails, check whether another process is using the port:

```bash
ss -ltnp | grep ':8390'
```

Only stop a process after identifying it and confirming that you are authorized to stop it.

### Start clients
Open a separate terminal for each client and run:

```bash
./client_2390
```

Register different usernames, for example:

```text
REGISTER Chamod
```

and in the second client:

```text
REGISTER Pabasara
```

The client may register automatically at startup depending on the implementation. Follow the actual prompts displayed by your client.

### Basic session
After registration, try:

```text
LIST
BCAST Hello everyone
PMSG Pabasara Hello Pabasara
JOIN IT
ROOMS
RMSG IT Hello room members
LEAVE IT
QUIT
```

Use usernames that are currently registered.

## 11. Protocol Reference

Commands and normal responses use newline-terminated text lines. `SENDFILE` is followed by raw file bytes and requires special framing.

| Command | Purpose | Typical successful response |
|---|---|---|
| `REGISTER <username>` | Register a username | `OK REGISTERED <username> NID:2123` |
| `LIST` | List registered users | `OK USERS <user-list> NID:2123` |
| `BCAST <message>` | Broadcast to other users | `OK SENT NID:2123` |
| `PMSG <username> <message>` | Send a private message | `OK SENT NID:2123` |
| `JOIN <room>` | Join or create a room | `OK JOINED <room> NID:2123` |
| `LEAVE <room>` | Leave a room | `OK LEFT <room> NID:2123` |
| `ROOMS` | List rooms | `OK ROOMS <room-list> NID:2123` |
| `RMSG <room> <message>` | Send a room message | `OK SENT NID:2123` |
| `SENDFILE <target> <filename> <filesize>` | Transfer a file | `OK FILE_RECEIVED <filename> NID:2123` |
| `QUIT` | Disconnect gracefully | `OK BYE NID:2123` |

Forwarded message examples:

```text
MSG BCAST <sender> <message>
MSG PRIV <sender> <message>
MSG ROOM <room> <sender> <message>
MSG FILE <sender> <filename> <filesize>
```

These are forwarded message notifications, not ordinary `OK`/`ERR` responses. Confirm exact output formatting against the implementation.

### Error responses

| Error | Meaning |
|---|---|
| `ERR 001 USERNAME_TAKEN` | Username is already registered |
| `ERR 002 USER_NOT_FOUND` | Target user is unavailable |
| `ERR 003 ROOM_NOT_FOUND` | Room does not exist or is unavailable |
| `ERR 004 FILE_TOO_LARGE` | File exceeds the configured size limit |
| `ERR 005 UNKNOWN_COMMAND` | Command is not recognized |

Normal responses should include the `NID:2123` suffix.

## 12. File Transfer

### Transfer format
The sender transmits a newline-terminated header:

```text
SENDFILE <target> <filename> <filesize>\n
```

Immediately after the header, the sender transmits exactly `<filesize>` raw bytes.

The server must read the header as a line and then consume exactly the stated number of bytes. It must not treat file bytes as commands or assume that one `recv()` call returns the entire file.

### File-size limit
The source code defines:

```c
#define MAX_FILE_SIZE (10LL * 1024 * 1024)
```

This is 10 MiB (10 × 1024 × 1024 bytes). Verify client and server behavior when a file exceeds the configured maximum.

### Storage location
The personalized storage root is:

```text
./storage/IT21212390/
```

The intended sender-specific layout is:

```text
./storage/IT21212390/<sender_username>/<filename>
```

Inspect stored files:

```bash
find storage/IT21212390 -type f -ls
```

### Verify file integrity
Create a small test file:

```bash
printf 'NetMessenger file sharing test - IT21212390\n' > test.txt
wc -c test.txt
```

Send it to another registered user:

```text
SENDFILE Chamod test.txt
```

If the receiver saves it as `received_test.txt`, compare the files:

```bash
cmp test.txt received_test.txt
```

No output from `cmp` indicates that the files have identical contents. Confirm that both files are from the same test.

## 13. Server Logging

The personalized log filename is:

```text
netmsg_IT21212390.log
```

Inspect the log after the server has generated events:

```bash
cat netmsg_IT21212390.log
```

or:

```bash
tail -n 50 netmsg_IT21212390.log
```

Useful events to record include:
- Server startup and listening port.
- Client connections and disconnections.
- Successful and failed registration.
- Broadcast and private messages.
- Room joins, leaves, and room messages.
- File-transfer start, storage, delivery, and failures.
- Invalid commands and other errors.

Log only events actually implemented. Avoid recording passwords, private file contents, or unnecessary sensitive data.

## 14. Testing and Verification

Run each test against the actual server and clients. Record observed results instead of assuming expected results occurred.

| ID | Test | Example input/action | What to verify |
|---|---|---|---|
| T01 | Server startup | Run `./server_2390` | Listening port is 8390 |
| T02 | Registration | `REGISTER Chamod` | Registration succeeds |
| T03 | Duplicate username | Register same name from another client | Duplicate is rejected |
| T04 | User listing | `LIST` | Registered users are listed |
| T05 | Broadcast | `BCAST Hello everyone` | Other clients receive message |
| T06 | Private message | `PMSG Pabasara Hello` | Target receives private message |
| T07 | Join room | `JOIN IT` | Client joins room |
| T08 | List rooms | `ROOMS` | Available rooms are listed |
| T09 | Room message | `RMSG IT Hello room` | Room members receive message |
| T10 | Leave room | `LEAVE IT` | Client leaves room |
| T11 | File transfer | `SENDFILE Chamod test.txt` | Target receives file |
| T12 | File integrity | Run `cmp` | No differences are reported |
| T13 | Unknown user | `PMSG Kamal Hello` | Correct error is returned |
| T14 | Unknown room | `RMSG UnknownRoom Hello` | Correct room error is returned |
| T15 | Unknown command | `HELLO` | Unknown-command response is returned |
| T16 | Unknown file target | `SENDFILE Kamal test.txt` | Error is returned and next command still works |
| T17 | Oversized file | Send file above configured maximum | File is safely rejected |
| T18 | Disconnect | `QUIT` | Client disconnects cleanly |
| T19 | Persistent logging | Inspect log after operations | Implemented events are recorded |
| T20 | Concurrent clients | Connect several clients | Concurrent handling works as expected |

Fill in the actual result and status for every test you execute. Capture genuine terminal screenshots for the report.

## 15. Troubleshooting

### Compilation errors
Run:

```bash
make -f Makefile_2390 clean
make -f Makefile_2390
```

Fix the first compiler error before addressing later errors that may result from it.

### Port already in use
Check:

```bash
ss -ltnp | grep ':8390'
```

Identify the process before taking action.

### Client cannot connect
Check that:
- The server is running.
- The client uses the correct IP address and port.
- The port is reachable from the client host.
- Firewall rules permit the connection if clients run on different hosts.

For a client running on the same machine, `127.0.0.1` is suitable if that matches the client source configuration.

### File not found
Check the client's current working directory:

```bash
pwd
ls -l test.txt
```

Use a filename that the client can open.

### File comparison reports differences
Verify that you are comparing the correct source and received files. Check sizes:

```bash
wc -c test.txt received_test.txt
```

If sizes differ, inspect the send/receive loops and exact-byte framing.

### Log file does not exist
The file may be created only after the first logged event. Confirm that `log_event()` is called and that the server can write to its working directory.

## 16. Security Considerations and Limitations

This project is a learning implementation, not a production-ready messaging service.

- TCP does not encrypt application data.
- Validate usernames, room names, filenames, and command lengths.
- Prevent path traversal and unintended file overwrites.
- Handle file-size limits and transfer failures without corrupting the TCP stream.
- Synchronize shared client and room state across threads.
- Handle partial socket reads/writes and disconnected clients.
- Avoid stale client state and incomplete files after interrupted transfers.
- Protect log files and avoid recording sensitive data.
- A file-size limit is not a substitute for complete input validation.
- Do not expose the service to untrusted networks without additional security controls.

Document limitations actually observed during testing.

## 17. Development Evidence

Keep the following evidence for the implementation report, as required by the assignment:
- Incremental Git commit history.
- Design diary describing implementation decisions and testing.
- AI prompt log containing actual substantive interactions used during development.
- Annotated screenshots of important code sections.
- Server and multiple-client execution screenshots.
- Evidence of registration, messaging, room operations, and file transfer.
- File-integrity verification output.
- Server log excerpt.
- Personalized port, NID, source filenames, and storage-path evidence.
- Completed test table with actual results.

Do not fabricate commit history, prompts, test results, or screenshots. Be prepared to explain socket operations, threading, shared-state synchronization, protocol framing, and file-transfer code.

## 18. Submission Checklist

- [ ] `server_2390.c` is included.
- [ ] `client_2390.c` is included.
- [ ] `Makefile_2390` is included.
- [ ] Both programs compile successfully.
- [ ] Required commands have been tested.
- [ ] Error cases have been tested.
- [ ] File transfer and file integrity have been verified.
- [ ] Personalization requirements have been checked.
- [ ] Design diary is included.
- [ ] AI prompt log reflects actual usage.
- [ ] Implementation report includes genuine evidence.
- [ ] Git history meets the assignment requirement.
- [ ] Final ZIP uses the required name: `IE3010_IT21212390.zip`.
- [ ] ZIP contents have been checked.

Example archive command (adjust the list to match the exact required deliverables):

```bash
zip -r IE3010_IT21212390.zip \
    server_2390.c \
    client_2390.c \
    Makefile_2390 \
    README.md \
    DESIGN_DIARY.md \
    AI_PROMPT_LOG.md \
    IMPLEMENTATION_REPORT.md \
    netmsg_IT21212390.log \
    storage
```

Inspect the archive:

```bash
unzip -l IE3010_IT21212390.zip
```

Do not include unnecessary binaries or test artifacts if the assignment prohibits them.

## 19. Author

**Student registration:** IT21212390  
**Project:** NetMessenger — Multi-Client Chat and File-Sharing Platform over TCP/IP  
**Module:** IE3010 — Network Programming

---

*This README documents the intended project design and usage. Verify all commands, output formats, features, and test results against the final source code before submission.*
