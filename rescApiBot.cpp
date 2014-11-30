// AUTHOR: Ray Powers
// DATE: November 25, 2014

// DESCRIPTION: RESC ApiBot will be a utility application to handle interacting with 
// 		 		RESTful APIs on the web. The goal is make it really easy to hook into 
//				new services


// Standard Library
#include<iostream>
#include<sstream>
#include<string>
#include<fstream>
#include<sstream>
#include<deque>
#include<csignal>
#include<ctime>

// Multithreading
#include<pthread.h>

// RESC Common library
#include "rescFramework.h"

// Setup our namespaces
using namespace std;
using namespace RESC;

// GLOBALS
int serverSocket;

// Data Structures
struct threadArgs {
	int serverSocket;
};

// Prototypes
string ReadFile(string filename);
// Function reads from file an api request.
// pre: none
// post: none

void ProcessServerMessages(int serverSocket);
// Function handles incoming messages from server
// pre: none
// post: none

void* ServerThread(void* args_p);
// Function is the entry point for interacting with the server
// pre: none
// post: none

void ProcessSignal(int sig);
// Function interprets signal interrupts so we can handle safe closure of threads.
// pre: none
// post: none

int main (int argc, char * argv[]) {

	// Need to grab Command-line arguments and convert them to useful types
	// Initialize arguments with proper variables.
	if (argc != 5){
		// Incorrect number of arguments
		cerr << "Incorrect number of arguments. Please try again." << endl;
		return -1;
	}
	// Need to store arguments
	string hostname = argv[1];
	unsigned short serverPort = atoi(argv[2]);
	
	// Process Interrupts so we can gracefully exit()
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ProcessSignal;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	
	serverSocket = OpenSocket(hostname, serverPort);
	
	string username = argv[3];
	string authMsg = username + "|test";
	SendMessage(serverSocket, authMsg);
	string authResponse = ReadMessage(serverSocket);
	
	if (CheckAuthResponse(authResponse)) {
		// Establish Socket Reader
		struct threadArgs* args_p = new threadArgs;
		args_p -> serverSocket = serverSocket;
		pthread_t tid;
		int threadStatus = pthread_create(&tid, NULL, ServerThread, (void*)args_p);
		if (threadStatus != 0) {
			cerr << "Failed to create child process." << endl;
			serverSocket = 0;
		}

		// Build command.
		time_t currentTime = time(NULL);
		stringstream tmp;
		tmp << currentTime;
		string filename = "output" + tmp.str() + ".txt";
		string tmpFile = "outputRecent.txt";
		tmp.str("");
		tmp.clear();

		string sysCommandPrep = "date +\"%F %H:%M:%S :\" > " + tmpFile;
		string sysCommand = "curl " + string(argv[4]) + " >> " + tmpFile;
		string sysCommandLog = "cat " + tmpFile + " >> " + filename;
		string sysCommandNewLine = "echo \"\n\" >> " + filename;
		while (true) {
			// Do work section
			// Grab url
			sleep(5);
			int status = system(sysCommandPrep.c_str());
			status = system(sysCommand.c_str());
			status = system(sysCommandLog.c_str());
			status = system(sysCommandNewLine.c_str());
		
			// Do work
			sleep(5);
			// Read File
			string data = ReadFile(tmpFile);
		
			// Send Message
 			data = "/msg ray /filestream "+ username + " " + data;
 			SendMessage(serverSocket, data);
 			cout << "Sent message." << endl;
		}
	}
	
	CloseSocket(serverSocket);

	return 0;
}

string ReadFile(string filename)
{
	stringstream ss;
	string result = "";
	ifstream srcFile;
	srcFile.open(filename.c_str());
	if (srcFile.fail()) {
		cout << "Error reading output file" << endl;
		//exit(-1);
	}
	if (srcFile.is_open()) {
		string tmp = "";
		while (srcFile >> tmp) {
			// check anything?
			ss << tmp;
		}
	}
	srcFile.close();
	srcFile.clear();
	
	return ss.str();
}

void ProcessServerMessages(int serverSocket)
{
  // Locals
  bool canRead = true;
  fd_set hostfd;
  struct timeval tv;

  // Clear FD_Set and set timeout.
  FD_ZERO(&hostfd);
  tv.tv_sec = 1;
  tv.tv_usec = 10000;

  // Initialize Data
  FD_SET(serverSocket, &hostfd);
  int numberOfSocks = serverSocket + 1;

  while (canRead) {
    // Read Data
    int pollSock = select(numberOfSocks, &hostfd, NULL, NULL, &tv);
    tv.tv_sec = 1;
    tv.tv_usec = 10000;
    FD_SET(serverSocket, &hostfd);
    if (pollSock != 0 && pollSock != -1) {
      // Socket has data, let's retrieve it.
      string incMessage = ReadMessage(serverSocket);
	  // Display Message
	  //cout << incMessage << endl;
    }
  }
}

void ProcessSignal(int sig) {
	cout << endl << "Shutting down ApiBot." << endl;
	sleep(1);
	string quit = "/quit";
	SendMessage(serverSocket, quit);
	CloseSocket(serverSocket);
	exit(0);
}

void* ServerThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int hostSock = tmp -> serverSocket;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Communicate with Client
  ProcessServerMessages(hostSock);

  // Quit thread
  pthread_exit(NULL);
}