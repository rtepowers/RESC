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
#include<csignal>

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
int conn_socket;
unordered_map<string, string> userRepository;
pthread_mutex_t UserRepositoryLock;
int UserRepositoryStatus = pthread_mutex_init(&UserRepositoryLock, NULL);

// Function Prototypes
void* requestThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

void ProcessSignal(int sig);
// Function interprets signal interrupts so we can handle safe closure of threads.
// pre: none
// post: none

bool ValidateUser(RES::RESCAuthRequest request);
// Function handles the Authentication portion of our system. Will need to update at
// some point
// pre: none
// post: none

using namespace std;

int main (int argc, char * argv[])
{
	// Process Arguments
	unsigned short serverPort; 
	if (argc != 2){
		// Incorrect number of arguments
		cerr << "Incorrect number of arguments. Please try again." << endl;
		return -1;
	}
	serverPort = atoi(argv[1]); 
	
	// Create socket connection
	conn_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
	
	// Process Interrupts so we can gracefully exit()
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ProcessSignal;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	
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
	RESC::RESCAuthRequest request = RESC::ReadAuthRequest(requestSock);
	if (ValidateUser(request)) {
		// Successfully validate.
		RESC::RESCAuthResult result;
		result.status = SUCCESSFUL_AUTH;
		RESC::SendAuthResult(requestSock, result);
	} else {
		RESC::RESCAuthResult result;
		result.status = INVALID_AUTH;
		RESC::SendAuthResult(requestSock, result);
	}
}

void ProcessSignal(int sig) {
	close(conn_socket);
	cout << endl << endl << "Shutting down server." << endl;
	exit(1);
}

bool ValidateUser(RESC::RESCAuthRequest request)
{
	bool isValidated = false;
	char uname[9];
	string pword[9];
	for (short i = 0; i < 9; i++) {
		uname[i] = request.encryptedData[i];
		pword[i] = request.encruptedData[i+9];
	}
	pthread_mutex_lock(&UserRepositoryLock);
	unordered_map<string, string>::const_iterator userIter = userRepository.find(uname);
	if (userIter == userRepository.end() {
		// We do not have this user.
		userRepository.insert(make_pair<string, string>(uname, pword));
		isValidated = true;
	} else {
		if ((*userIter).second == pword) {
			// Passwords match
			isValidated = true;
		}
	}
	ptherad_mutex_unlock(&UserRepositoryLock);
	return isValidated;
}