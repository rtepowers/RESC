// AUTHOR: Ray Powers
// DATE: October 25, 2014
// FILE: rescNode.cpp

// DESCRIPTION: RescNode.cpp will be the v2 replacement of rescServer.cpp.
// 				It will take advantage of a lot less clutter in code and
//				will attempt to bring low latency to serving messages.

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

int main (int argc, int * argv[])
{

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
	//RESCMessage = RESC::ReadRequest(requestSock);
}