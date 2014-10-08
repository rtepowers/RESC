// AUTHOR: Raymond Powers
// DATE: October 3, 2014
// PLATFORM: C++ 

// DESCRIPTION: Ray's Easily Scaled Chat Client
//              A simply to use and straight forward terminal app.

// Standard Library
#include<iostream>
#include<sstream>
#include<string>
#include<cstring>

// Network Function
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>

// User Interface
#include<curses.h>

// Multithreading
#include<pthread.h>

using namespace std;

// GLOBALS
const int INPUT_LINES = 3;
const char ENTER_SYM = '\n';
const char BACKSPACE_SYM = '\b';
const int DELETE_SYM = 127;
WINDOW *INPUT_SCREEN;
WINDOW *MSG_SCREEN;
fd_set hostfd;
struct timeval tv;
pthread_mutex_t displayLock;
int displayStatus = pthread_mutex_init(&displayLock, NULL);

// Data Structures
struct threadArgs {
  int serverSock;
};

// Function Prototypes
void clearInputScreen();
// Function resets InputScreen to default state.
// pre: InputScreen should exist.
// post: none

bool getUserInput(string& inputStr, bool isPwd);
// Function tests whether the user has typed a message.
// pre: InputScreen should exist.
// post: none

void prepareWindows();
// Function prepares user interface
// pre: none
// post: none

bool SendMessage(int HostSock, string msg);
// Function sends message to Host socket.
// pre: HostSock should exist.
// post: none

string GetMessage(int HostSock, int messageLength);
// Function retrieves message from Host socket.
// pre: HostSock should exist.
// post: none

bool SendInteger(int HostSock, int hostInt);
// Function sends a network long variable over the network.
// pre: HostSock must exist
// post: none

long GetInteger(int HostSocks);
// Function listens to socket for a network Long variable.
// pre: HostSock must exist.
// post: none

void* clientThread(void* args_p);
// Function serves as the entry point to a new thread.
// pre: none
// post: none

int openSocket (string hostName, unsigned short serverPort);
// Function sets up a working socket to use in sending data.
// pre: none
// post: none

void displayMsg(string& msg);
// Function displays message
// pre: ChatScreen should exist.
// post: none

void DisplayData (int hostSock);
// Function loops over polling a socket for data.
// pre: none
// post: none


int main (int argNum, char* argValues[]) {

  // Locals
  string inputStr;
  string hostname;
  string username = "";
  unsigned short serverPort;
  int hostSock;
  int numberOfSocks;

  // Need to grab Command-line arguments and convert them to useful types
  // Initialize arguments with proper variables.
  if (argNum != 3){
    // Incorrect number of arguments
    cerr << "Incorrect number of arguments. Please try again." << endl;
    return -1;
  }

  // Need to store arguments
  hostname = argValues[1];
  serverPort = atoi(argValues[2]);

  // Begin User Interface
  prepareWindows();

  // Establish a socket Connection
  hostSock = openSocket(hostname, serverPort+1);
  
  // Get A Chat Server 
  string identifier = "CLIENT";
  SendInteger(hostSock, identifier.length()+1);
  SendMessage(hostSock, identifier);
  long serverNameLength = GetInteger(hostSock);
  string serverName = GetMessage(hostSock, serverNameLength);
  close(hostSock);
  
  int serverSock = openSocket(serverName, serverPort);
  
  // Login State
  // TODO: Figure out Authentication?
  string welcomeMsg = "\nWelcome to RESC!\n\n";
  displayMsg(welcomeMsg);
  wrefresh(INPUT_SCREEN);
  
  // Establish a Thread to handle displaying new messages.
  struct threadArgs* args_p = new threadArgs;
  args_p -> serverSock = serverSock;
  pthread_t tid;
  int threadStatus = pthread_create(&tid, NULL, clientThread, (void*)args_p);
  if (threadStatus != 0){
    // Failed to create child thread
    cerr << "Failed to create child process." << endl;
  }//*/

  if (hostSock > 0 ) {
    // Enter a loop to begin
    while (true) {
      // If the user finished typing a message, get it and process it.
      if (getUserInput(inputStr, false)) {
		// If it's a command, handle it.
		if (inputStr == "/quit" || inputStr == "/exit" || inputStr == "/close") {
		  // TODO: Notify that we're done.
		  break;
		}
		
		// Display message in chat window
		string tmp = "You said: ";
		tmp.append(inputStr);
		tmp.append("\n");
		displayMsg(tmp);
		
		// Send to Server
		if (!SendInteger(serverSock, inputStr.length()+1)) {
		  cerr << "Unable to send Int. " << endl;
		  break;
		}

		if (!SendMessage(serverSock, inputStr)) {
		  cerr << "Unable to send Message. " << endl;
		  break;
		}
		
		// Clean slate
		inputStr.clear();
		clearInputScreen();
      }
    }
	// Send to Server
	if (!SendInteger(serverSock, inputStr.length()+1)) {
	  cerr << "Unable to send Int. " << endl;
	}

	if (!SendMessage(serverSock, inputStr)) {
	  cerr << "Unable to send Message. " << endl;
	}
  }//*/

  // Clean up and Close things down.
  delwin(INPUT_SCREEN);
  delwin(MSG_SCREEN);
  endwin();
  close(hostSock);

  exit(-1);
}

bool SendMessage(int HostSock, string msg) {

  // Local Variables
  int msgLength = msg.length()+1;
  char msgBuff[msgLength];
  strcpy(msgBuff, msg.c_str());
  msgBuff[msgLength-1] = '\0';

  // Since they now know how many bytes to receive, we'll send the message
  int msgSent = send(HostSock, msgBuff, msgLength, 0);
  if (msgSent != msgLength){
    // Failed to send
    cerr << "Unable to send data. Closing clientSocket: " << HostSock << "." << endl;
    return false;
  }

  return true;
}

string GetMessage(int HostSock, int messageLength) {

  // Retrieve msg
  int bytesLeft = messageLength;
  char buffer[messageLength];
  char* buffPTR = buffer;
  while (bytesLeft > 0){
    int bytesRecv = recv(HostSock, buffPTR, messageLength, 0);
    if (bytesRecv <= 0) {
      // Failed to Read for some reason.
      cerr << "Could not recv bytes. Closing clientSocket: " << HostSock << "." << endl;
      return "";
    }
    bytesLeft = bytesLeft - bytesRecv;
    buffPTR = buffPTR + bytesRecv;
  }

  return buffer;
}

long GetInteger(int HostSock) {

  // Retreive length of msg
  int bytesLeft = sizeof(long);
  long networkInt;
  char* bp = (char *) &networkInt;
  
  while (bytesLeft) {
    int bytesRecv = recv(HostSock, bp, bytesLeft, 0);
    if (bytesRecv <= 0){
      // Failed to receive bytes
      cerr << "Failed to receive bytes. Closing clientSocket: " << HostSock << "." << endl;
      return -1;
    }
    bytesLeft = bytesLeft - bytesRecv;
    bp = bp + bytesRecv;
  }
  return ntohl(networkInt);
}

bool SendInteger(int HostSock, int hostInt) {

  // Local Variables
  long networkInt = htonl(hostInt);

  // Send Integer (as a long)
  int didSend = send(HostSock, &networkInt, sizeof(long), 0);
  if (didSend != sizeof(long)){
    // Failed to Send
    cerr << "Unable to send data. Closing clientSocket: " << HostSock << "."  << endl;
    return false;
  }

  return true;
}

void prepareWindows() {

  // Initialize Screen
  initscr();
  start_color();

  // Set timeout of input to 2 miliseconds.
  halfdelay(1);
  //nodelay(INPUT_SCREEN, true);

  // Create new MSG_SCREEN window and enable scrolling of text.
  noecho();
  cbreak();
  MSG_SCREEN = newwin(LINES - INPUT_LINES, COLS, 0, 0);
  scrollok(MSG_SCREEN, TRUE);
  wsetscrreg(MSG_SCREEN, 0, LINES - INPUT_LINES - 1);
  wrefresh(MSG_SCREEN);

  // SET Colors for window.
  init_pair(1, COLOR_CYAN, COLOR_BLACK);
  wbkgd(MSG_SCREEN, COLOR_PAIR(1));

  // Create new INPUT_SCREEN window. No scrolling.
  INPUT_SCREEN = newwin(INPUT_LINES, COLS, LINES - INPUT_LINES, 0);
  
  // Prepare Input Screen.
  clearInputScreen();
  
}

void clearInputScreen() {

  // Clear screen and write a division line.
  werase(INPUT_SCREEN);
  mvwhline(INPUT_SCREEN, 0, 0, '-', COLS);

  // Move cursor to the start of the next line.
  wmove(INPUT_SCREEN, 1, 0);
  waddstr(INPUT_SCREEN, "Input:  ");

  wrefresh(INPUT_SCREEN);

}

int openSocket (string hostName, unsigned short serverPort) {

  // Local variables.
  struct hostent* host;
  int status;
  int bytesRecv;

  // Create a socket and start server communications.
  int hostSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (hostSock <= 0) {
    // Socket was unsuccessful.
    cerr << "Socket was unable to be opened." << endl;
    return -1;
  }

  // Get host IP and Set proper fields
  host = gethostbyname(hostName.c_str());
  if (!host) {
    cerr << "Unable to resolve hostname's ip address. Exiting..." << endl;
    return -1;
  }
  char* tmpIP = inet_ntoa( *(struct in_addr *)host->h_addr_list[0]);
  unsigned long serverIP;
  status = inet_pton(AF_INET, tmpIP,(void*) &serverIP);
  if (status <= 0) return -1;

  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = serverIP ;
  serverAddress.sin_port = htons(serverPort);

  // Now that we have the proper information, we can open a connection.
  status = connect(hostSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
  if (status < 0) {
    cerr << "Error with the connection." << endl;
    return -1;
  }

  return hostSock;
}

void* clientThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int hostSock = tmp -> serverSock;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Communicate with Client
  DisplayData(hostSock);

  // Close Client socket
  close(hostSock);

  // Quit thread
  pthread_exit(NULL);
}

void DisplayData (int hostSock) {

  // Locals
  bool canRead = true;
  fd_set hostfd;
  struct timeval tv;

  // Clear FD_Set and set timeout.
  FD_ZERO(&hostfd);
  tv.tv_sec = 1;
  tv.tv_usec = 10000;

  // Initialize Data
  FD_SET(hostSock, &hostfd);
  int numberOfSocks = hostSock + 1;

  while (canRead) {
    // Read Data
    int pollSock = select(numberOfSocks, &hostfd, NULL, NULL, &tv);
    tv.tv_sec = 1;
    tv.tv_usec = 10000;
    FD_SET(hostSock, &hostfd);
    if (pollSock != 0 && pollSock != -1) {
      long msgLength = GetInteger(hostSock);
      if (msgLength <= 0) {
		cerr << "Couldn't get integer from Server." << endl;
		break;
      }
      string clientMsg = GetMessage(hostSock, msgLength);
      if (clientMsg == "") {
		cerr << "Couldn't get message from Server." << endl;
		break;
      }
      displayMsg(clientMsg);
      wrefresh(INPUT_SCREEN);
    }
  }
}

void displayMsg(string &msg) {
  pthread_mutex_lock(&displayLock);
  // Add Msg to screen.
  if (msg.c_str()[0] == '/') {
    string blank = "\n";
    waddstr(MSG_SCREEN, blank.c_str());
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    wbkgd(MSG_SCREEN, COLOR_PAIR(2));
    
  } else {
    // SET Colors for window.
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    wbkgd(MSG_SCREEN, COLOR_PAIR(1));
  }
  waddstr(MSG_SCREEN, msg.c_str());
  //waddch(MSG_SCREEN, '\n');
  
  // Show new screen.
  wrefresh(MSG_SCREEN);
  pthread_mutex_unlock(&displayLock);

}

bool getUserInput(string& inputStr, bool isPwd) {

  // Locals
  bool success = false;
  int userText;

  // Get input from from the input screen. Timeout will occur if no characters are available.
  userText = wgetch(INPUT_SCREEN);

  // Did we return a proper value?
  if (userText == ERR)
    return false;

  // Can we display the text? Add to inputStr if yes.
  if (isprint(userText)) {
    inputStr += (char)userText;
    if (!isPwd) {
      waddch(INPUT_SCREEN, userText);
    } else {
      char COVER = '*';
      waddch(INPUT_SCREEN, COVER);
    }
  }
  // Pressing <enter> signals that the user has 'sent' something.
  else {
    // Allow for Backspacing.
    if (userText == BACKSPACE_SYM || userText == DELETE_SYM) {
      if (inputStr.length() > 0) {
		inputStr.replace(inputStr.length()-1, 1, "");
		wmove(INPUT_SCREEN, 1, 8 + inputStr.length());
		userText = 32;
		waddch(INPUT_SCREEN, userText);
		wmove(INPUT_SCREEN, 1, 8 + inputStr.length());
      }

      // Show new screen.
      wrefresh(INPUT_SCREEN);
    }
    
    if (userText == ENTER_SYM) {
	  // If inputStr isn't empty, it should be submitted.
	  if (inputStr.size() > 0) {
		success = true;
      }
    }
  }
  return success;
}