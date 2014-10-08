// AUTHOR: Raymond Powers
// DATE: October 3, 2014
// PLATFORM: C++ 

// DESCRIPTION: Ray's Easily Scaled Chat Server

// Standard Library
#include<iostream>
#include<sstream>
#include<string>
#include<ctime>
#include<cstdlib>

// Network Functions
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>

// Multithreading
#include<pthread.h>

using namespace std;

// DATA TYPES
struct threadArgs {
  int clientSock;
};

struct User {
// TODO: Determine what a User is. Classify?
};

struct Msg {
// TODO: Determine what a Msg is. Classify?
};

// GLOBALS
const int MAXPENDING = 20;


// Function Prototypes
void* clientThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: closes socket

void ProcessMessage(int clientSock);
// Function handles chat portion of application.
// pre: clientSock must be established
// post: none

int openSocket (string hostName, unsigned short serverPort);
// Function sets up a working socket to use in sending data.
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

int main(int argNum, char* argValues[]){

  // Process Arguments
  unsigned short serverPort; 
  string trackerHostName;
  // Need to grab Command-line arguments and convert them to useful types
  // Initialize arguments with proper variables.
  if (argNum != 3){
    // Incorrect number of arguments
    cerr << "Incorrect number of arguments. Please try again." << endl;
    return -1;
  }
  trackerHostName = argValues[1];
  serverPort = atoi(argValues[2]); 

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
  
  int trackerSock = openSocket(trackerHostName, serverPort+1);
  string identifier = "SERVER";
  SendInteger(trackerSock, identifier.length()+1);
  SendMessage(trackerSock, identifier);
  close(trackerSock);
  
  cout << endl << endl << "RESC SERVER: Ready to accept connections. " << endl;

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

  // Handle chat
  ProcessMessage(clientSock);

  // Close Client socket
  close(clientSock);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessMessage(int clientSock) {
	// Locals
	string clientMsg = "";
	fd_set clientfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Clear FD_Set and set timeout.
	FD_ZERO(&clientfd);
	tv.tv_sec = 2;
	tv.tv_usec = 100000;

	// Initialize Data
	FD_SET(clientSock, &clientfd);
	sockIndex = clientSock + 1;
	
	while (clientMsg != "/quit") {
		// Send Data
		if (clientMsg != "") {
			string response = "Server has listened!\n";
			SendInteger(clientSock, response.length()+1);
			SendMessage(clientSock, response);
			clientMsg.clear();
		}
		
		// Read Data
		int pollSock = select(sockIndex, &clientfd, NULL, NULL, &tv);
		tv.tv_sec = 1;
		tv.tv_usec = 100000;
		FD_SET(clientSock, &clientfd);
		if (pollSock != 0 && pollSock != -1) {
		  string tmp;
		  long msgLength = GetInteger(clientSock);
		  if (msgLength <= 0) {
			cerr << "Couldn't get integer from Client." << endl;
			break;
		  }
		  
		  clientMsg = GetMessage(clientSock, msgLength);
		  if (clientMsg == "") {
			cerr << "Couldn't get message from Client." << endl;
			break;
		  }
		  tmp = "Client Said: ";
		  tmp.append(clientMsg);
		  cout << tmp << endl;
		  tmp.clear();
		  
		}
	}
	  cout << "Closing Thread." << endl;
}

int openSocket (string hostName, unsigned short serverPort) {

  // Local variables.
  struct hostent* host;
  int status;
  int bytesRecv;

  // Create a socket and start server communications.
  int hostSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (hostSock <= 0) {
    // Socket was unsuccessful.
    cerr << "Socket was unable to be opened." << endl;
    return -1;
  }

  // Get host IP and Set proper fields
  host = gethostbyname(hostName.c_str());
  if (!host) {
    cerr << "Unable to resolve hostname's ip address. Exiting..." << endl;
    return -1;
  }
  char* tmpIP = inet_ntoa( *(struct in_addr *)host->h_addr_list[0]);
  unsigned long serverIP;
  status = inet_pton(AF_INET, tmpIP,(void*) &serverIP);
  if (status <= 0) return -1;

  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = serverIP ;
  serverAddress.sin_port = htons(serverPort);

  // Now that we have the proper information, we can open a connection.
  status = connect(hostSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
  if (status < 0) {
    cerr << "Error with the connection." << endl;
    return -1;
  }

  return hostSock;
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