// AUTHOR: Ray Powers
// DATE: October 25, 2014
// FILE: rescClient2.cpp

// DESCRIPTION: RescClient2.cpp is a v2 replacement of rescClient.cpp.
//				This app will improve upon the performance of the previous version.
//				Will also have greater functionality. (Userlist?)

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

using namespace std;

int main (int argc, char * argv[])
{
	// Need to grab Command-line arguments and convert them to useful types
	// Initialize arguments with proper variables.
	if (argc != 3){
		// Incorrect number of arguments
		cerr << "Incorrect number of arguments. Please try again." << endl;
		return -1;
	}
	// Need to store arguments
	string hostname;
	unsigned short serverPort;
	hostname = argv[1];
	serverPort = atoi(argv[2]);
	
	// Open ServerSocket
	int serverSocket = RESC::OpenSocket(hostname, serverPort);
	
	RESC::RESCMessage request;
	RESC::RESCHeader header;
	header.cmd = RESC::RESCClientMessage;
	header.dest[0] = '\0';
	header.source[0] = '\0';
	request.hdr = header;
	for (int i =0 ; i < 255; i++) {
		request.msg[i] = 's';
	}
	request.msg[255] = '\0';
	RESC::SendRequest(serverSocket, request);
	
	return 0;
}