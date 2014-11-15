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
unordered_map<int, deque<RESC::Message> > MSG_QUEUE;
pthread_mutex_t MsgQueueLock;
int msgQueueStatus = pthread_mutex_init(&MsgQueueLock, NULL);
unordered_map<string, int> USER_LIST;
pthread_mutex_t UserListLock;
int usgListStatus = pthread_mutex_init(&UserListLock, NULL);
unordered_map<string, string> USER_REPO;
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

void GetUserList(string destUser);
// Function Grabs list of users online
// pre: none
// post: none

bool ProcessMessage(RESC::Message msg);
// Function processess incoming messages.
// pre: none
// post: none

void ProcessSignal(int sig);
// Function interprets signal interrupts so we can handle safe closure of threads.
// pre: none
// post: none

bool ValidateUser(RESC::Message request, RESC::RESCUser &user);
// Function checks user request for proper credentials
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
	RESC::RESCUser user;
	fd_set requestfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Authenticate User
	RESC::Message authRequest;
	bool hasValidated;
	do {
		authRequest = RESC::ReadMessage(requestSock);
		if (authRequest.hdr.msgType == RESC::INVALID_MSG) return;
		hasValidated = ValidateUser(authRequest, user);
		if (!hasValidated) {
			// Response
			RESC::Message authResponse = RESC::CreateAuthResponse(0, false);
			RESC::SendMessage(requestSock, authResponse);
		}
	} while (!hasValidated);
	RESC::Message authResponse = RESC::CreateAuthResponse(user.id, true);
	RESC::SendMessage(requestSock, authResponse);
	
	// Announce User, Update UserLists
	
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
			RESC::Message msg = RESC::ReadMessage(requestSock);
			if (!ProcessMessage(msg)) break;
		}
		
		// Send Data
		pthread_mutex_lock(&MsgQueueLock);
		unordered_map<int, deque<RESC::Message> >::iterator msgIter = MSG_QUEUE.find(user.id);
		if (msgIter != MSG_QUEUE.end()) {
			while (!(*msgIter).second.empty()) {
				SendMessage(requestSock, (*msgIter).second.front());
				(*msgIter).second.pop_front();
			}
		}
		pthread_mutex_unlock(&MsgQueueLock);
	}	
}

void GetUserList(string destUser) {

}



bool ProcessMessage(RESC::Message msg) {
	if (msg.hdr.msgType == RESC::INVALID_MSG) return false;
	
	unordered_map<int, deque<RESC::Message> >::iterator msgIter;
	switch(msg.hdr.msgType) {
		case RESC::BROADCAST_MSG:
			pthread_mutex_lock(&MsgQueueLock);
				// Add to all the queues
				msgIter = MSG_QUEUE.begin();
				while (msgIter != MSG_QUEUE.end()) {
					(*msgIter).second.push_back(msg);
					msgIter++;
				}
			pthread_mutex_unlock(&MsgQueueLock);
			return true;
			break;
		case RESC::DIRECT_MSG:
			pthread_mutex_lock(&MsgQueueLock);
				// Add to all the queues
				msgIter = MSG_QUEUE.find(msg.hdr.toUserId);
				if (msgIter != MSG_QUEUE.end()) {
					(*msgIter).second.push_back(msg);
				}
			pthread_mutex_unlock(&MsgQueueLock);
			return true;
			break;
		case RESC::USERLIST_MSG:
			return true;
			break;
		default:
			break;
	}
	
	return false;
}

bool ValidateUser(RESC::Message request, RESC::RESCUser &user)
{
	bool isValidated = false;
	stringstream ss;
	string username;
	string password;
	
	if (request.hdr.msgType == RESC::AUTH_MSG) {
		for (int i = 0; i < request.body.length(); i++) {
			if (request.body[i] == '|') {
				// found the divider
				username = ss.str();
				ss.str("");
				ss.clear();
			} else {
				ss << request.body[i];
			}
		}
		password = ss.str();
		ss.str("");
		ss.clear();
		pthread_mutex_lock(&UserRepositoryLock);
		unordered_map<string, string>::iterator userIter = USER_REPO.find(username);
		if (userIter != USER_REPO.end()) {
			if (!password.compare((*userIter).second)) {
				isValidated = true;
			}
		} else {
			// We do not have this user. Add new user
			USER_REPO.insert(make_pair<string, string>(username, password));
			isValidated = true;
		}
		pthread_mutex_unlock(&UserRepositoryLock);
	}
	
	if (isValidated) {
		user.username = username;
		pthread_mutex_lock(&UserListLock);
		user.id = USER_LIST.size() + 1;
		USER_LIST.insert(make_pair<string, int>(user.username, user.id));
		pthread_mutex_unlock(&UserListLock);
	}
	
	return isValidated;
}

void ProcessSignal(int sig) {
	close(conn_socket);
	cout << endl << endl << "Shutting down server." << endl;
	exit(1);
}