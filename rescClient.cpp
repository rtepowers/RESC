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
#include<queue>

// User Interface
#include<curses.h>

// Multithreading
#include<pthread.h>

// RESC Framework
#include "rescFramework.h"

using namespace std;
using namespace RESC;

// Globals
const int INPUT_LINES = 3;
const int USER_COLS = 10;
const char ENTER_SYM = '\n';
const char BACKSPACE_SYM = '\b';
const int DELETE_SYM = 127; // NOTE: Now symbol for this.
WINDOW* INPUT_SCREEN;
WINDOW* MSG_SCREEN;
WINDOW* USER_SCREEN;
pthread_mutex_t displayLock;
int displayStatus = pthread_mutex_init(&displayLock, NULL);
deque<string> USER_LIST;
pthread_mutex_t userListLock;
int userListStatus = pthread_mutex_init(&userListLock, NULL);

// Data Structures
struct threadArgs {
	int serverSocket;
};

// Function Prototypes
void ClearInputScreen();
// Function resets INPUT_SCREEN to default state
// pre: none
// post: none

void ClearUserScreen();
// Function resets the USER_SCREEN to default state
// pre: none
// post: none

bool GetUserInput(string &inputStr, bool shouldProtect);
// Function checks whether the user has typed a message.
// pre: inputStr should exist. 
// post: inputStr will be modified when the return value is true.

void PrepareWindows();
// Function prepares the user interface
// pre: none
// post: none

void* ServerThread(void* args_p);
// Function is the entry point for interacting with the server
// pre: none
// post: none

void DisplayMessage(string &msg);
// Function displays message on screen
// pre: MSG_SCREEN should exist
// post: none

void DisplayUserList(string &msg);
// Function displays the list of Users
// pre: USER_SCREEN should exist
// post: none

void ProcessIncomingData(int serverSocket);
// Function loops over polling the server socket for data.
// pre: none
// post: none

bool HasAuthenticated (int serverSocket, User &user);
// Function handles authentication with the server.
// pre: none
// post: none

void ProcessMessage(string rawMsg);
// Function processess incoming messages.
// pre: none
// post: none

int main (int argc, char * argv[])
{
	// LOCALS
	string inputStr;
	string userName;
	User user;
	
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
	
	// Begin User Interface
	PrepareWindows();
	
	// Connect To Chat Server (BOOTSTRAP?)
	int serverSocket = OpenSocket(hostname, serverPort);
	while (!HasAuthenticated(serverSocket, user)) {
	}
	userName = user.username;
	
	// Establish Socket Reader
	struct threadArgs* args_p = new threadArgs;
	args_p -> serverSocket = serverSocket;
	pthread_t tid;
	int threadStatus = pthread_create(&tid, NULL, ServerThread, (void*)args_p);
	if (threadStatus != 0) {
		cerr << "Failed to create child process." << endl;
		serverSocket = 0;
	}
	if (serverSocket > 0) {
		// Enter Function Loop
		while (true) {
			if (GetUserInput(inputStr, false)) {
				// Process Local Commands
				if (inputStr == "/quit" || inputStr == "/exit" || inputStr == "/close") {
					// Notify Server we're done.
					SendMessage(serverSocket, inputStr);
					break;
				}
			
				// Provide some feedback to the user.
				string tmp = "You said: ";
				tmp.append(inputStr);
				tmp.append("\n");
				DisplayMessage(tmp);
			
				// Send to Chat Server
				SendMessage(serverSocket, inputStr);
				
				// Clean slate
				inputStr.clear();
				ClearInputScreen();
			}
		}
	}
	
	// Clean it all up
	CloseSocket(serverSocket);
	delwin(INPUT_SCREEN);
	delwin(USER_SCREEN);
	delwin(MSG_SCREEN);
    endwin();

	exit(0);
}

void PrepareWindows() {

  // Initialize Screen
  initscr();
  start_color();

  // Set timeout of input to 1 miliseconds.
  halfdelay(1);
  //nodelay(INPUT_SCREEN, true);

  // Create new MSG_SCREEN window and enable scrolling of text.
  noecho();
  cbreak();
  MSG_SCREEN = newwin(LINES - INPUT_LINES, COLS - USER_COLS, 0, 0);
  scrollok(MSG_SCREEN, TRUE);
  wsetscrreg(MSG_SCREEN, 0, LINES - INPUT_LINES - 1);
  wrefresh(MSG_SCREEN);

  // SET Colors for window.
  init_pair(1, COLOR_MAGENTA, COLOR_BLACK);
  wbkgd(MSG_SCREEN, COLOR_PAIR(1));

  // Create new INPUT_SCREEN window. No scrolling.
  INPUT_SCREEN = newwin(INPUT_LINES, COLS, LINES - INPUT_LINES, 0);
  
  // Prepare Input Screen.
  ClearInputScreen();
  
  USER_SCREEN = newwin(LINES - INPUT_LINES, USER_COLS, 0, COLS - USER_COLS);
  werase(USER_SCREEN);
  mvwvline(USER_SCREEN, 0, 0, '|', LINES - INPUT_LINES);
  wrefresh(USER_SCREEN);
  
}

void ClearInputScreen() {
	// Clear screen and write a division line.
	werase(INPUT_SCREEN);
	mvwhline(INPUT_SCREEN, 0,0,'-', COLS);
	
	// Move Cursor to start of next line
	wmove(INPUT_SCREEN, 1, 0);
	waddstr(INPUT_SCREEN, "Input:  ");
	
	wrefresh(INPUT_SCREEN);
}

void ClearUserScreen() {
	// Clear screen and write a division line.
	werase(USER_SCREEN);
	mvwvline(USER_SCREEN, 0, 0, '|', LINES - INPUT_LINES);
	wrefresh(INPUT_SCREEN);
}

bool HasAuthenticated (int serverSocket, User &user) {

  // Locals
  stringstream ss;
  for (short i =0; i < COLS - USER_COLS; i++) {
  	ss << "/";
  }
  string loginMsg = ss.str() + "\nWelcome to RESC!\n\nPlease login.\n\n";
  ss.str("");
  ss.clear();
  string clearScr = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
  string pwdMsg = "/\b\nPlease enter your password.\n";
  string userName;
  string userPwd;

  // Reset Screen
  DisplayMessage(clearScr);
  wrefresh(INPUT_SCREEN);
  ClearInputScreen();

  // Get UserName
  DisplayMessage(loginMsg);
  wrefresh(INPUT_SCREEN);
  while (!GetUserInput(userName, false)) {
  }
  ClearInputScreen();
  
  // Get Password
  DisplayMessage(pwdMsg);
  wrefresh(INPUT_SCREEN);
  while (!GetUserInput(userPwd, true)) {
  }
  ClearInputScreen();

  // Reset Screen
  DisplayMessage(clearScr);
  wrefresh(INPUT_SCREEN);
  ClearInputScreen();
  
  // Process
  ss << userName << "|" << userPwd;
  string bodyMsg = ss.str();
  ss.str("");
  ss.clear();
  SendMessage(serverSocket, bodyMsg);
  string authResponse = ReadMessage(serverSocket);

  // Evaluate Host Response
  if (CheckAuthResponse(authResponse)) {
    // Login Sucessful!
    user.username = userName;
    return true;
  }
  // Login Failed
  return false;
}

bool GetUserInput(string &inputStr, bool shouldProtect) {

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
    if (!shouldProtect) {
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

void DisplayMessage(string &msg) {
  pthread_mutex_lock(&displayLock);
  // Add Msg to screen.
  // SET Colors for window.
  init_pair(1, COLOR_CYAN, COLOR_BLACK);
  wbkgd(MSG_SCREEN, COLOR_PAIR(1));
  waddstr(MSG_SCREEN, msg.c_str());
  
  // Show new screen.
  wrefresh(MSG_SCREEN);
  pthread_mutex_unlock(&displayLock);

}

void DisplayUserList(string &msg) {
	pthread_mutex_lock(&displayLock);
 	init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
	wbkgd(USER_SCREEN, COLOR_PAIR(2));
	waddstr(USER_SCREEN, msg.c_str());
	
	// Show screen
	wrefresh(USER_SCREEN);
	pthread_mutex_unlock(&displayLock);
}

void* ServerThread(void* args_p) {
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int hostSock = tmp -> serverSocket;
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Communicate with Client
  ProcessIncomingData(hostSock);

  // Close Client socket
  close(hostSock);

  // Quit thread
  pthread_exit(NULL);
}

void ProcessIncomingData(int serverSocket) {

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
	  ProcessMessage(incMessage);
	  //DisplayMessage(incMessage);
	  wrefresh(INPUT_SCREEN);
    }
  }
}

void ProcessMessage(string rawMsg) {
	RESC::Message msg = RESC::ConvertServerMessage(rawMsg);
	switch(msg.cmd) {
		case USER_LIST_MSG:
			ClearUserScreen();
			DisplayUserList(msg.msg);
			break;
		default:
			DisplayMessage(rawMsg);
			break;
	}
}