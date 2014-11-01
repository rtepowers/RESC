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
#include<unordered_map>

// Multithreading
#include<pthread.h>

// RESC Framework
#include "rescFramework.h"

using namespace std;
using namespace RESC;

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
	string hostname = argv[1];
	unsigned short serverPort = atoi(argv[2]);
	
	// Open ServerSocket
	int serverSocket = OpenSocket(hostname, serverPort);
	
	// Process
	RESCAuthRequest request;
	string uname = "raysTest";
	string pword = "testing";
	for (short i = 0; i < 9; i++) {
		request.encryptedData[i] = (i > uname.length()) ? '\0' : uname[i];
		request.encryptedData[i+9] = (i > pword.length()) ? '\0' : pword[i];
	}
	
	CloseSocket(serverSocket);
	return 0;
}
