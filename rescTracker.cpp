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

// RESC Library
#include "Common.h"

using namespace std;
using namespace RESC;

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
deque<Msg> MsgQueue;
pthread_mutex_t MsgQueueLock;
int MsgQueueStatus = pthread_mutex_init(&MsgQueueLock, NULL);
unordered_map<string, Server> ServerMap;
pthread_mutex_t ServerMapLock;
int ServerMapStatus = pthread_mutex_init(&ServerMapLock, NULL);

// Function Prototypes
void* clientThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

void ProcessRequest(int requestSock);
// Function process incoming request whether from client or server
// pre: none
// post: none

void AddClientMessage(Msg message);
// Function adds messages to MsgQueue
// pre: none
// post: none

string GetClientMessages(string serverName);
// Function grabs messages from other servers.
// pre: none
// post: none

void BroadcastMessage(string message, string serverName);
// Function handles broadcasting messages.
// pre: none
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
		pthread_mutex_unlock(&ServerListLock);
		
		// Send back the hostname
		cout << "Sending client the hostname: " << svrTmp.hostName << "!" << endl;
		SendInteger(requestSock, svrTmp.hostName.length()+1);
		SendMessage(requestSock, svrTmp.hostName);
		return;
	}
	
	// Otherwise, request is a Server
	long nameLength = GetInteger(requestSock);
	string hostname = GetMessage(requestSock, nameLength);
	
	// Finally add this server to pool
	Server newSvr;
	newSvr.hostName = hostname;
	pthread_mutex_lock(&ServerListLock);
		ServerList.push(newSvr);
		cout << "Adding " << newSvr.hostName << " or " << hostname << " to pool!" << endl;
	pthread_mutex_unlock(&ServerListLock);
	pthread_mutex_lock(&ServerMapLock);
		ServerMap.insert(make_pair<string, Server>(hostname, newSvr));
	pthread_mutex_unlock(&ServerMapLock);
	
	// should pass messages from queue
	// Locals
	fd_set trackerfd;
	struct timeval tv;
	int sockIndex = 0;
	
	// Clear FD_Set and set timeout.
	FD_ZERO(&trackerfd);
	tv.tv_sec = 2;
	tv.tv_usec = 100000;

	// Initialize Data
	FD_SET(requestSock, &trackerfd);
	sockIndex = requestSock + 1;
	
	while (true) {
		// Send Data
		// Grab from MsgQueue
		string message = GetClientMessages(newSvr.hostName);
		if (message != "") {
			if (!SendInteger(requestSock, message.length()+1)) {
				cerr << "Failed to send int. " << endl;
				break;
			}
			if (!SendMessage(requestSock, message)) {
				cerr << "Failed to send Message. " << endl;
				break;
			}
		}
	
		// Read Data
		int pollSock = select(sockIndex, &trackerfd, NULL, NULL, &tv);
		tv.tv_sec = 1;
		tv.tv_usec = 100000;
		FD_SET(requestSock, &trackerfd);
		if (pollSock != 0 && pollSock != -1) {
		  long msgLength = GetInteger(requestSock);
		  if (msgLength <= 0) {
			cerr << "Couldn't get integer from Client." << endl;
			break;
		  }
	  
		  string trackerMsg = GetMessage(requestSock, msgLength);
		  if (trackerMsg == "") {
			cerr << "Couldn't get message from Client." << endl;
			break;
		  }
		  cout << "Message from Tracker: " << trackerMsg << endl;
		  BroadcastMessage(trackerMsg, newSvr.hostName);
		}
	}
	cout << "Closing Thread." << endl;
	
	// Now send list of Peers 
}

void AddClientMessage(Msg message) {
	pthread_mutex_lock(&MsgQueueLock);
	MsgQueue.push_back(message);
	pthread_mutex_unlock(&MsgQueueLock);
}

string GetClientMessages(string serverName) {
	stringstream ss;
	pthread_mutex_lock(&MsgQueueLock);
	for (int i = 0; i < MsgQueue.size(); i++) {
		if (MsgQueue[i].To == serverName) {
			ss << MsgQueue[i].Text;
			MsgQueue.erase(MsgQueue.begin()+i);
			i--;
			break;
		}
	}
	pthread_mutex_unlock(&MsgQueueLock);
	return ss.str();
}

void BroadcastMessage(string message, string serverName) {
	Msg tmp;
	tmp.From = serverName;
	tmp.Text = message;
	tmp.Command = "/all";
	pthread_mutex_lock(&ServerMapLock);
	unordered_map<string, Server>::iterator got = ServerMap.begin();
    for ( ; got != ServerMap.end(); got++) {
      if ((got)->second.hostName != serverName) {
		// Send them a message.
		tmp.To = (got)->second.hostName;
		AddClientMessage(tmp);
      }
    }
	pthread_mutex_unlock(&ServerMapLock);
}