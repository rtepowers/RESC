# Ray's Easily Scaled Chat -- RESC
---
### Description:
RESC is an attempt at making a simple and easy to use communication platform. My goal is to enable it to scale for lots of connected users. As of right now I have the system capable of chat communication and file streaming via the ApiBot. The next big milestone is make the ApiBot an on Demand resource for content.  
-Ray

### Dependencies:  
##### Curses.h:  
(Ubuntu Precise @ sudo apt-get install libncurses5-dev)

### Compile:  

Mac OS X
```bash
make
```

Linux (Ubuntu Precise)
```bash
g++ rescClient.cpp -o rescClient -pthread -lcurses -std=c++0x
g++ rescServer.cpp -o rescServer -pthread -std=c++0x
g++ rescApiBot.cpp -o rescApiBot -pthread -std=c++0x
```

### Usage:

Running the server:
```bash
./rescServer <port number>
```
Running the Client
```bash
./rescClient <server hostname/IP> <port number> 
```

Running the API Bot (Runs GET requests only)
```bash
./rescApiBot <server hostname/ip> <port number> <bot username> <url of api>
```

### Protocol:

RESC follows two simple patterns for sending messages.  
1. */all (message)*    : Send a message to all connected users. Default message type and will assume this type if not specified.
2. */msg (username) (message)*     : Send a message to a specific user

