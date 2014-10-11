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

// RESC Library
#include "Common.h"

using namespace std;
using namespace RESC;

// DATA TYPES
struct threadArgs {
  int clientSock;
};

// GLOBALS
const int MAXPENDING = 20;


// Function Prototypes
void* clientThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: closes socket

void ProcessClient(int clientSock);
// Function handles chat portion of application.
// pre: clientSock must be established
// post: none

void* trackerThread(void* args_p);
// Function serves as the entry point to a new tracker thread.
// pre: none
// post: closes socket

void ProcessTracker(int trackerSock);
// Function handles chat organization.
// pre: trackerSock must be established
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
  char currentHostName_c[128];
  gethostname(currentHostName_c, sizeof currentHostName_c);
  string currentHostName = currentHostName_c;
  SendInteger(trackerSock, currentHostName.length()+1);
  SendMessage(trackerSock, currentHostName);
  struct threadArgs* args_t = new threadArgs;
  args_t -> clientSock = trackerSock;
  pthread_t trackID;
  int trackStatus = pthread_create(&trackID, NULL, trackerThread, (void*)args_t);
  if (trackStatus != 0) {
  	// Failed to create tracker thread
  	cerr << "Failed to open tracker thread." << endl;
  	close(trackerSock);
  	pthread_exit(NULL);
  }
  
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
  ProcessClient(clientSock);

  // Close Client socket
  close(clientSock);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessClient(int clientSock) {
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

void* trackerThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int trackerSock = tmp -> clientSock;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Handle chat
  ProcessTracker(trackerSock);

  // Close Client socket
  close(trackerSock);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessTracker(int trackerSock) {
	// Locals
	fd_set trackerfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Clear FD_Set and set timeout.
	FD_ZERO(&trackerfd);
	tv.tv_sec = 2;
	tv.tv_usec = 100000;

	// Initialize Data
	FD_SET(trackerSock, &trackerfd);
	sockIndex = trackerSock + 1;
	
	while (true) {
		// Send Data
		// Grab from MsgQueue
		
		// Read Data
		int pollSock = select(sockIndex, &trackerfd, NULL, NULL, &tv);
		tv.tv_sec = 1;
		tv.tv_usec = 100000;
		FD_SET(trackerSock, &trackerfd);
		if (pollSock != 0 && pollSock != -1) {
		  long msgLength = GetInteger(trackerSock);
		  if (msgLength <= 0) {
			cerr << "Couldn't get integer from Client." << endl;
			break;
		  }
		  
		  string trackerMsg = GetMessage(trackerSock, msgLength);
		  if (trackerMsg == "") {
			cerr << "Couldn't get message from Client." << endl;
			break;
		  }
		  cout << "Message from Tracker: " << trackerMsg << endl;
		}
	}
	  cout << "Closing Thread." << endl;
}