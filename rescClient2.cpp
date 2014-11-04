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

// User Interface
#include<curses.h>

// Multithreading
#include<pthread.h>

// RESC Framework
#include "rescFramework.h"

using namespace std;
using namespace RESC;

// Global Constants
const int INPUT_LINES = 3;
const int USER_COLS = 9;
const char ENTER_SYM = '\n';
const char BACKSPACE_SYM = '\b';
const int DELETE_SYM = 127; // NOTE: Now symbol for this.
WINDOW* INPUT_SCREEN;
WINDOW* MSG_SCREEN;
WINDOW* USER_SCREEN;
fd_set HOSTFD;
struct timeval TV;
pthread_mutex_t displayLock;
int displayStatus = pthread_mutex_init(&displayLock, NULL);

// Data Structures
struct threadArgs {
	int serverSocket;
};

// Function Prototypes
void ClearInputScreen();
// Function resets INPUT_SCREEN to default state
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

void ProcessIncomingData(int serverSocket);
// Function loops over polling the server socket for data.
// pre: none
// post: none

bool HasAuthenticated (int serverSocket, RESCUser &user);
// Function handles authentication with the server.
// pre: none
// post: none

int main (int argc, char * argv[])
{
	// LOCALS
	string inputStr;
	RESCUser user;
	
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
	
	// Connect To Chat Server
	int serverSocket = OpenSocket(hostname, serverPort);
	
	while (!HasAuthenticated(serverSocket, user)) {
	}
	
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
  mvwvline(USER_SCREEN, 0, 0, '|', LINES - INPUT_LINES);
  
}

void ClearInputScreen() {
	// Clear screen and write a division line.
	werase(INPUT_SCREEN);
	mvwhline(INPUT_SCREEN, 0,0,'-', COLS);
	
	// Move Cursor to start of next line
	wmove(INPUT_SCREEN, 1, 0);
	waddstr(INPUT_SCREEN, "Input: ");
	
	wrefresh(INPUT_SCREEN);
}

bool HasAuthenticated (int serverSocket, RESCUser &user) {

  // Locals
  string loginMsg = "//////\nWelcome to RESC!\n\nPlease login.\n\n";
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
	RESCAuthRequest request;
	for (short i = 0; i < 9; i++) {
		request.encryptedData[i] = (i > userName.length()) ? '\0' : userName[i];
		user.username[i] = request.encryptedData[i];
		request.encryptedData[i+9] = (i > userPwd.length()) ? '\0' : userPwd[i];
	}
	RESCAuthResult result = Authorize(serverSocket, request);

  // Evaluate Host Response
  if (result.status == SUCCESSFUL_AUTH) {
    // Login Sucessful!
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
  if (msg.c_str()[0] == '/') {
    string blank = "\n";
    waddstr(MSG_SCREEN, blank.c_str());
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    wbkgd(MSG_SCREEN, COLOR_PAIR(2));
    
  } else {
    // SET Colors for window.
    init_pair(1, COLOR_MAGENTA, COLOR_BLACK);
    wbkgd(MSG_SCREEN, COLOR_PAIR(1));
  }
  waddstr(MSG_SCREEN, msg.c_str());
  
  // Show new screen.
  wrefresh(MSG_SCREEN);
  pthread_mutex_unlock(&displayLock);

}