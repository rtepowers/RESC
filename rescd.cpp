// AUTHOR: Ray Powers
// DATE: October 25, 2014
// FILE: RESCD - Daemon tool for the RESC system.

// DESCRIPTION: RESCD will be v2 of the rescTracker. This v2 will improve throughput
// 				with a low latency design with SOA. 

// Standard Library
#include<iostream>
#include<sstream>
#include<string>
#include<ctime>
#include<cstdlib>
#include<queue>
#include<unordered_map>

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

// RESC Framework
#include "rescFramework.h"

// DATA TYPES
struct threadArgs {
  int requestSocket;
};

// Globals
int MAXPENDING = 20;
deque<RESC::RESCServer*> serverList;
unordered_map<string, RESC::RESCServer*> serverDetails;
pthread_mutex_t ServerListLock;
pthread_mutex_t ServerDetailsLock;
int ServerListStatus = pthread_mutex_init(&ServerListLock, NULL);
int ServerDetailsStatus = pthread_mutex_init(&ServerDetailsLock, NULL);

// Function Prototypes
void* requestThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

using namespace std;

int main (int argc, char * argv[])
{
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
	
	// We are good to go! Alert admin running that we can now accept requests
	cout << endl << "RESC Tracker: Ready to accept connections. " << endl;
	
	// Accept connections
	while (true) {
		// Accept connections
		struct sockaddr_in clientAddress;
		socklen_t addrLen = sizeof(clientAddress);
		int requestSock = accept(conn_socket, (struct sockaddr*) &clientAddress, &addrLen);
		if (requestSock < 0) {
			cerr << "Error accepting connections." << endl;
			exit(-1);
		}

		// Create child thread to handle process
		struct threadArgs* args_p = new threadArgs;
		args_p -> requestSocket = requestSock;
		pthread_t tid;
		int threadStatus = pthread_create(&tid, NULL, requestThread, (void*)args_p);
		if (threadStatus != 0){
			// Failed to create child thread
			cerr << "Failed to create child process." << endl;
			close(requestSock);
			pthread_exit(NULL);
		}

	}

	return 0;
}


void* requestThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int requestSocket = tmp -> requestSocket;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Handle Request
  ProcessRequest(requestSocket);

  // Close Client socket
  close(requestSocket);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessRequest(int requestSock) {

	// Parse messages
	RESC::RESCMessage* request = RESC::ReadRequest(requestSock);
	
	cout << "Message was : " << request -> msg << endl;
}