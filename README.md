# NetMessenger - IE3010 -

Registration Number: IT21212390

## Personalisation
- Registration: IT21212390
- Numeric part: 21212390
- Last four digits: 2390
- TCP port: 8390
- NID: NID:2123
- Server: server_2390.c
- Client: client_2390.c
- Makefile: Makefile_2390
- Log: netmsg_IT21212390.log
- Storage root: ./storage/IT21212390/
- ZIP: IE3010_IT21212390.zip

## Step 2 features
- REGISTER
- LIST
- BCAST
- Presence JOINED/LEFT notifications
- QUIT
- Multiple simultaneous clients using pthreads
- Duplicate username detection

## Build
make -f Makefile_2390

## Run
Terminal 1: ./server_2390
Terminal 2+: ./client_2390

## Tests
LIST example: `OK USERS alice,bob NID:2123`
BCAST sender receives: `OK SENT NID:2123`
Other clients receive: `MSG BCAST alice Hello everyone`
Presence: `MSG PRESENCE JOINED bob` and `MSG PRESENCE LEFT bob`

Note: the Step 2 client is sequential. A later step can add a receiving thread for asynchronous messages.
