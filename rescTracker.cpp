// AUTHOR: Raymond Powers
// DATE: October 7, 2014
// PLATFORM: C++ 

// DESCRIPTION: Ray's Easily Scaled Chat Tracker
//              This application will handle Server initiation and Client
//              load balancing.

// Standard Library
#include<iostream>
#include<sstream>
#include<string>
#include<ctime>
#include<cstdlib>
#include<queue>

// Network Functions
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>

// Multithreading
#include<pthread.h>

using namespace std;

// DATA TYPES
struct threadArgs {
  int clientSock;
};

struct Server {
	string hostName;
};

// GLOBALS
const int MAXPENDING = 20;
queue<Server> ServerList;
pthread_mutex_t ServerListLock;
int ServerStatus = pthread_mutex_init(&ServerListLock, NULL);

// Function Prototypes
void* clientThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

bool SendMessage(int HostSock, string msg);
// Function sends message to Host socket.
// pre: HostSock should exist.
// post: none

string GetMessage(int HostSock, int messageLength);
// Function retrieves message from Host socket.
// pre: HostSock should exist.
// post: none

bool SendInteger(int HostSock, int hostInt);
// Function sends a network long variable over the network.
// pre: HostSock must exist
// post: none

long GetInteger(int HostSocks);
// Function listens to socket for a network Long variable.
// pre: HostSock must exist.
// post: none

int main(int argc, char* argv[]){

  // Process Arguments
  unsigned short serverPort; 
  if (argc != 2){
    // Incorrect number of arguments
    // TODO: Could probably have better argument checking. BoostLib?
    cerr << "Incorrect number of arguments. Please try again." << endl;
    return -1;
  }
  serverPort = atoi(argv[1]); 

  // Create socket connection
  int conn_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (conn_socket < 0){
    cerr << "Error with socket." << endl;
    exit(-1);
  }

  // Set the socket Fields
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;    // Always AF_INET
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddress.sin_port = htons(serverPort);
  
  // Assign Port to socket
  int sock_status = bind(conn_socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
  if (sock_status < 0) {
    cerr << "Error with bind." << endl;
    exit(-1);
  }

  // Set socket to listen.
  int listen_status = listen(conn_socket, MAXPENDING);
  if (listen_status < 0) {
    cerr << "Error with listening." << endl;
    exit(-1);
  }
  cout << endl << endl << "RESC Tracker: Ready to accept connections. " << endl;

  // Accept connections
  while (true) {
    // Accept connections
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(clientAddress);
    int clientSocket = accept(conn_socket, (struct sockaddr*) &clientAddress, &addrLen);
    if (clientSocket < 0) {
      cerr << "Error accepting connections." << endl;
      exit(-1);
    }

    // Create child thread to handle process
    struct threadArgs* args_p = new threadArgs;
    args_p -> clientSock = clientSocket;
    pthread_t tid;
    int threadStatus = pthread_create(&tid, NULL, clientThread, (void*)args_p);
    if (threadStatus != 0){
      // Failed to create child thread
      cerr << "Failed to create child process." << endl;
      close(clientSocket);
      pthread_exit(NULL);
    }
    
  }

  return 0;
}

void* clientThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int clientSock = tmp -> clientSock;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Handle Request
  ProcessRequest(clientSock);

  // Close Client socket
  close(clientSock);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessRequest(int requestSock) {
	// Local Variables
	
	// Need to identify request
	long identityLength = GetInteger(requestSock);
	if (identityLength <= 0) {
		cerr << "Failed reading socket: " << requestSock << endl;
		return;
	}
	string identifierType = GetMessage(requestSock, identityLength);
	if (identifierType != "CLIENT" && identifierType != "SERVER") {
		cerr << "Failed to identify request at socket: " << requestSock << endl;
		return;
	}
	
	// For Clients
	if (identifierType == "CLIENT") {
		Server svrTmp;
		svrTmp.hostName = "ERROR";
		pthread_mutex_lock(&ServerListLock);
		// Cycle list of servers so we have a RoundRobin scheme
		if (ServerList.empty()) {
			// No servers here.
		} else {
			// Take the front.
			svrTmp = ServerList.front();
			ServerList.pop();
			ServerList.push(svrTmp);
		}
		
		cout << "Sending client the hostname: " << svrTmp.hostName << "!" << endl;
		pthread_mutex_unlock(&ServerListLock);
		
		// Send back the hostname
		SendInteger(requestSock, svrTmp.hostName.length()+1);
		SendMessage(requestSock, svrTmp.hostName);
		return;
	}
	
	// Otherwise, request is a Server
	// Let's grab their IP address
	socklen_t len;
	struct sockaddr_storage addr;
	char ipstr[INET6_ADDRSTRLEN];
	int port;
	len = sizeof addr;
	getpeername(requestSock, (struct sockaddr*)&addr, &len);
	// deal with IPv4:
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		port = ntohs(s->sin_port);
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	}
	
	char hostname[128];

	gethostname(hostname, sizeof hostname);
	
	// Finally add this server to pool
	Server newSvr;
	newSvr.hostName = ipstr;
	pthread_mutex_lock(&ServerListLock);
		ServerList.push(newSvr);
		cout << "Adding " << newSvr.hostName << " or " << hostname << " to pool!" << endl;
	pthread_mutex_unlock(&ServerListLock);
	
	// Now send list of Peers 
}

bool SendMessage(int HostSock, string msg) {

  // Local Variables
  int msgLength = msg.length()+1;
  char msgBuff[msgLength];
  strcpy(msgBuff, msg.c_str());
  msgBuff[msgLength-1] = '\0';

  // Since they now know how many bytes to receive, we'll send the message
  int msgSent = send(HostSock, msgBuff, msgLength, 0);
  if (msgSent != msgLength){
    // Failed to send
    cerr << "Unable to send data. Closing clientSocket: " << HostSock << "." << endl;
    return false;
  }

  return true;
}

string GetMessage(int HostSock, int messageLength) {

  // Retrieve msg
  int bytesLeft = messageLength;
  char buffer[messageLength];
  char* buffPTR = buffer;
  while (bytesLeft > 0){
    int bytesRecv = recv(HostSock, buffPTR, messageLength, 0);
    if (bytesRecv <= 0) {
      // Failed to Read for some reason.
      cerr << "Could not recv bytes. Closing clientSocket: " << HostSock << "." << endl;
      return "";
    }
    bytesLeft = bytesLeft - bytesRecv;
    buffPTR = buffPTR + bytesRecv;
  }

  return buffer;
}

long GetInteger(int HostSock) {

  // Retreive length of msg
  int bytesLeft = sizeof(long);
  long networkInt;
  char* bp = (char *) &networkInt;
  
  while (bytesLeft) {
    int bytesRecv = recv(HostSock, bp, bytesLeft, 0);
    if (bytesRecv <= 0){
      // Failed to receive bytes
      cerr << "Failed to receive bytes. Closing clientSocket: " << HostSock << "." << endl;
      return -1;
    }
    bytesLeft = bytesLeft - bytesRecv;
    bp = bp + bytesRecv;
  }
  return ntohl(networkInt);
}

bool SendInteger(int HostSock, int hostInt) {

  // Local Variables
  long networkInt = htonl(hostInt);

  // Send Integer (as a long)
  int didSend = send(HostSock, &networkInt, sizeof(long), 0);
  if (didSend != sizeof(long)){
    // Failed to Send
    cerr << "Unable to send data. Closing clientSocket: " << HostSock << "."  << endl;
    return false;
  }

  return true;
}