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

using namespace std;

// DATA TYPES
struct threadArgs {
  int requestSocket;
};

// Globals
int MAXPENDING = 20;
int conn_socket;
unordered_map<string, deque<RESC::RESCMessage> > MSG_QUEUE;
pthread_mutex_t MsgQueueLock;
int msgQueueStatus = pthread_mutex_init(&MsgQueueLock, NULL);

// Function Prototypes
void* requestThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

void ProcessMessage(RESC::RESCMessage msg);
// Function processess incoming messages.
// pre: none
// post: none

void ProcessSignal(int sig);
// Function interprets signal interrupts so we can handle safe closure of threads.
// pre: none
// post: none

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
	
	// We are good to go! Alert admin running that we can now accept requests
	cout << endl << "RESCD: Ready to accept connections. " << endl;
	
	
	// Process Interrupts so we can gracefully exit()
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ProcessSignal;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	
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

	// Polling structures
	string uname = "";
	fd_set requestfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Clear FD_Set and set timeout.
	FD_ZERO(&requestfd);
	tv.tv_sec = 2;
	tv.tv_usec = 100000;

	// Initialize Data
	FD_SET(requestSock, &requestfd);
	sockIndex = requestSock + 1;
	
	while (true) {
		// Read Data
		int pollSock = select(sockIndex, &requestfd, NULL, NULL, &tv);
		tv.tv_sec = 1;
		tv.tv_usec = 100000;
		FD_SET(requestSock, &requestfd);
		if (pollSock != 0 && pollSock != -1) {
			// READ DATA
			RESC::RESCMessage msg = RESC::ReadMessage(requestSock);
			if (msg.jobType != RESC::INVALID_MSG) {
				// Process this
				if (uname == "") {
					uname = string(msg.source);
				}
				string message = string(msg.data);
				cout << "Converted Message is: " << message << endl;
				if (message == "/quit" || message == "/close" || message == "/exit") {
					break;
				}
				ProcessMessage(msg);
			}
		}
		
		// Send Data
		pthread_mutex_lock(&MsgQueueLock);
		// Find user's Msg queue
		unordered_map<string, deque<RESC::RESCMessage> >::iterator jobIter = MSG_QUEUE.find(uname);
		if (jobIter == MSG_QUEUE.end()) {
			// Unable to find msg queue... Add it?
			MSG_QUEUE.insert(make_pair<string, deque<RESC::RESCMessage> >(uname, deque<RESC::RESCMessage>()));
		} else {
			// Found msg queue. Let's add to it!
			for (int i = 0; i <jobIter -> second.size(); i++) {
				RESC::SendMessage(requestSock, jobIter -> second[i]);
			}
			jobIter -> second.clear();
		}
		pthread_mutex_unlock(&MsgQueueLock);
		
	}
	
	// Remove from MSG_QUEUE
	pthread_mutex_lock(&MsgQueueLock);
	// Find user's Msg queue
	unordered_map<string, deque<RESC::RESCMessage> >::iterator jobIter = MSG_QUEUE.find(uname);
	if (jobIter == MSG_QUEUE.end()) {
		// Unable to Find MSG_QUEUE, so they never built one.
	} else {
		MSG_QUEUE.erase(uname);
	}
	pthread_mutex_unlock(&MsgQueueLock);
}

void ProcessMessage(RESC::RESCMessage msg) {
	// Switch on message type
	if (msg.jobType == RESC::INVALID_MSG) {
		return;
	}
	string dest = string(msg.dest);
	string source = string(msg.source);
	if (msg.jobType == RESC::BROADCAST_MSG) {
		// Add to all Users
		pthread_mutex_lock(&MsgQueueLock);
		unordered_map<string, deque<RESC::RESCMessage> >::iterator queIter = MSG_QUEUE.begin();
		for (; queIter != MSG_QUEUE.end(); queIter++) {
			if (queIter ->first != source) {
				queIter -> second.push_back(msg);
			}
		}
		pthread_mutex_unlock(&MsgQueueLock);
	} else if (msg.jobType == RESC::DIRECT_MSG) {
		// Add to user's queue
		pthread_mutex_lock(&MsgQueueLock);
		// Find user's Msg queue
		unordered_map<string, deque<RESC::RESCMessage> >::iterator jobIter = MSG_QUEUE.find(dest);
		if (jobIter == MSG_QUEUE.end()) {
			// Unable to find msg queue... Add it?
			MSG_QUEUE.insert(make_pair<string, deque<RESC::RESCMessage> >(dest, deque<RESC::RESCMessage>()));
			jobIter = MSG_QUEUE.find(dest);
			jobIter -> second.push_back(msg);
		} else {
			// Found msg queue. Let's add to it!
			jobIter -> second.push_back(msg);
		}
		pthread_mutex_unlock(&MsgQueueLock);
	}
}

void ProcessSignal(int sig) {
	close(conn_socket);
	cout << endl << endl << "Shutting down server." << endl;
	exit(1);
}