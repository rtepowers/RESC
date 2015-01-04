// AUTHOR: Ray Powers
// DATE: October 25, 2014
// FILE: RESCServer - Daemon tool for the RESC system.

// DESCRIPTION: RESCServer will be v2 of the rescTracker. This v2 will improve throughput
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
unordered_map<string, deque<RESC::Message> > MSG_QUEUE;
pthread_mutex_t MsgQueueLock;
int msgQueueStatus = pthread_mutex_init(&MsgQueueLock, NULL);
unordered_map<string, RESC::User> USER_LIST;
pthread_mutex_t UserListLock;
int usgListStatus = pthread_mutex_init(&UserListLock, NULL);

// Function Prototypes
void* requestThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

void UpdateUserLists();
// Function sends all users an updated userlist
// pre: none
// post: none

void ProcessMessage(string rawMsg, string fromUser);
// Function processess incoming messages.
// pre: none
// post: none

void ProcessSignal(int sig);
// Function interprets signal interrupts so we can handle safe closure of threads.
// pre: none
// post: none

bool ValidateUser(string request, RESC::User &user);
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
	RESC::User user;
	fd_set requestfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Authenticate User
	bool hasValidated = true;
	do {
		string authRequest = RESC::ReadMessage(requestSock);
		hasValidated = ValidateUser(authRequest, user);
		string authResponse = (hasValidated) ? "SUCCESSFUL" : "UNSUCCESSFUL";
		cout << "User auth'd " << authResponse << endl;
		RESC::SendMessage(requestSock, authResponse);
	} while (!hasValidated);
	UpdateUserLists();
	
	// Announce User, Update UserLists
	
	// Clear FD_Set and set timeout.
	FD_ZERO(&requestfd);
	tv.tv_sec = 1;
	tv.tv_usec = 20000;

	// Initialize Data
	FD_SET(requestSock, &requestfd);
	sockIndex = requestSock + 1;
	
	while (true) {
		// Read Data
		int pollSock = select(sockIndex, &requestfd, NULL, NULL, &tv);
		tv.tv_sec = 1;
		tv.tv_usec = 20000;
		FD_SET(requestSock, &requestfd);
		if (pollSock != 0 && pollSock != -1) {
			// READ DATA
			string msg = RESC::ReadMessage(requestSock);
			if (RESC::HasQuit(msg)){
				break;
			}
			ProcessMessage(msg, user.username);
		}
		
		// Send Data
		pthread_mutex_lock(&MsgQueueLock);
		unordered_map<string, deque<RESC::Message> >::iterator msgIter = MSG_QUEUE.find(user.username);
		if (msgIter != MSG_QUEUE.end()) {
			while (!(*msgIter).second.empty()) {
				RESC::Message tmpMsg =  (*msgIter).second.front();
				// 
				string outMsg = RESC::EncodeMessage(tmpMsg);
				RESC::SendMessage(requestSock, outMsg);
				(*msgIter).second.pop_front();
			}
		}
		pthread_mutex_unlock(&MsgQueueLock);
	}	
	pthread_mutex_lock(&MsgQueueLock);
	MSG_QUEUE.erase(user.username);
	pthread_mutex_unlock(&MsgQueueLock);
	pthread_mutex_lock(&UserListLock);
		unordered_map<string, RESC::User>::iterator usrIter = USER_LIST.find(user.username);
		if (usrIter != USER_LIST.end()) {
			// found user
			user.isConnected = false;
			(*usrIter).second.isConnected = false;
		}
	pthread_mutex_unlock(&UserListLock);
	UpdateUserLists();
	cout << "Closing socket" << endl;
}

void UpdateUserLists() {
	stringstream ss;
	pthread_mutex_lock(&UserListLock);
		int count = 0;
		unordered_map<string, RESC::User>::iterator usrIter = USER_LIST.begin();
		while (usrIter != USER_LIST.end()) {
			if ((*usrIter).second.isConnected) {	
				if (count < 20) {		
					ss << "| " << (*usrIter).first << endl;
				}
				count++;
			}
			usrIter++;
		}
		ss << "| " << "-----" << endl;
		ss << "| " << count << " Users" << endl;
	pthread_mutex_unlock(&UserListLock);
	
	RESC::Message usrListMsg;
	usrListMsg.from = "SERVER";
	usrListMsg.cmd = RESC::USER_LIST_MSG;
	usrListMsg.msg = ss.str();
	ss.str("");
	ss.clear();
	unordered_map<string, deque<RESC::Message> >::iterator msgIter;
	pthread_mutex_lock(&MsgQueueLock);
	// Add to all the queues
	msgIter = MSG_QUEUE.begin();
	while (msgIter != MSG_QUEUE.end()) {
		(*msgIter).second.push_back(usrListMsg);
		msgIter++;
	}
	pthread_mutex_unlock(&MsgQueueLock);
}

void ProcessMessage(string rawMsg, string userFrom) {
	RESC::Message msg = RESC::CreateMessage(rawMsg, userFrom);
	if (msg.cmd == RESC::INVALID_MSG) return;
	
	unordered_map<string, deque<RESC::Message> >::iterator msgIter;
	switch(msg.cmd) {
		case RESC::BROADCAST_MSG:
			pthread_mutex_lock(&MsgQueueLock);
				// Add to all the queues
				msgIter = MSG_QUEUE.begin();
				while (msgIter != MSG_QUEUE.end()) {
					if ((*msgIter).first.compare(userFrom)) {
						(*msgIter).second.push_back(msg);
					}
					msgIter++;
				}
			pthread_mutex_unlock(&MsgQueueLock);
			break;
		case RESC::DIRECT_MSG:
			pthread_mutex_lock(&MsgQueueLock);
				msgIter = MSG_QUEUE.find(msg.to);
				if (msgIter != MSG_QUEUE.end()) {
					if ((*msgIter).first.compare(userFrom)) {
						(*msgIter).second.push_back(msg);
					}
				}
			pthread_mutex_unlock(&MsgQueueLock);
			break;
		case RESC::FILE_STREAM_MSG:
			pthread_mutex_lock(&MsgQueueLock);
				msgIter = MSG_QUEUE.find(msg.to);
				if (msgIter != MSG_QUEUE.end()) {
					if ((*msgIter).first.compare(userFrom)) {
						(*msgIter).second.push_back(msg);
					}
				}
			pthread_mutex_unlock(&MsgQueueLock);
			break;
		default:
			break;
	}
}

bool ValidateUser(string request, RESC::User &user)
{
	bool isValidated = false;
	stringstream ss;
	string username;
	string password;

	cout << "Validating user request: " << request << endl;
	const char * cMsg = request.c_str();
	for (int i = 0; i < request.length(); i++) {
		if (cMsg[i] == '|') {
			username = ss.str();
			ss.str("");
			ss.clear();
		}
		ss << cMsg[i];
	}
	password = ss.str();
	ss.str("");
	ss.clear();
	
	pthread_mutex_lock(&UserListLock);
	unordered_map<string, RESC::User>::iterator usrIter = USER_LIST.find(username);
	if (usrIter != USER_LIST.end()) {
		// found user
		if (!(*usrIter).second.password.compare(password)) {
			isValidated = true;
			user.username = username;
			user.isConnected = true;
			(*usrIter).second.isConnected = true;
		}
	} else {
		// New user, so create them
		RESC::User newUser;
		newUser.username = username;
		newUser.password = password;
		newUser.isConnected = true;
		user.username = username;
		user.isConnected = true;
		USER_LIST.insert(make_pair(username, newUser));
		isValidated = true;
	}
	pthread_mutex_unlock(&UserListLock);
	
	if (isValidated) {
		pthread_mutex_lock(&MsgQueueLock);
		MSG_QUEUE.insert(make_pair(username, deque<RESC::Message>()));
		pthread_mutex_unlock(&MsgQueueLock);
	}
	
	return isValidated;
}

void ProcessSignal(int sig) {
	close(conn_socket);
	cout << endl << endl << "Shutting down server." << endl;
	exit(1);
}