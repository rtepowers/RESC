// AUTHOR: Raymond Powers
// DATE: October 3, 2014
// PLATFORM: C++ 

// DESCRIPTION: RESC Common Utilities

#ifndef _COMMON_H_
#define _COMMON_H_

// Standard Library
#include<iostream>

// Network Function
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>

using namespace std;

namespace RESC {

// CORE DATA TYPES
	struct Msg {
		string To;
		string From;
		string Text;
		string Command;
	};

	struct User {
		string username;
	};
// CORE DATA HELPERS
	Msg ProcessText(string message) {
	  // Turn
	  // "/msg user blahblahbah"
	  // into
	  // (blahblahblah, /msg, user)
	  
	  Msg tmp;
	  const char * message_c = message.c_str();

	  if (message_c[0] == '/') {
		// Was a Command, Let's figure out what it was.
		int cmdSize = 0;
		for (int i = 1; i < message.length(); i++) {
		  if (message_c[i] == ' ') {
			cmdSize = i;
			break;
		  }
		}
		if (cmdSize == 0) {
		  // must be a non-argument command.
		  cmdSize = message.length();
		}
		// Build Command name
		for (int i = 0; i < cmdSize; i++) {
		  stringstream ss;
		  ss << message_c[i];
		  tmp.Command = ss.str();
		}
		
		// Determine the To User. 
		int userSize = 0;
		if (tmp.Command == "/msg") {
		  // Need to grab user information.
		  for (int i = cmdSize+1; i < message.length(); i++) {
			if (message_c[i] == ' ' ) {
			  userSize = i;
			  break;
			}
		  }
		  if (userSize == 0) {
			// 
			userSize = message.length();
		  }

		  // Build the To portion
		  for (int i = cmdSize+1; i < userSize; i++) {
			stringstream ss;
			ss << message_c[i];
			tmp.To = ss.str();
		  }
	
		  // Set our values (the 3 is including the characters for / and the two spaces
		  message.replace(0, userSize+cmdSize-3, "");
		  tmp.Text = message;
		}
	  } else {
		tmp.To = "all";
		tmp.Command = "/all";
	  }
	  
	  return tmp;
	}


// NETWORK HELPERS
	long GetInteger(int inSock) {

	  // Retreive length of msg
	  int bytesLeft = sizeof(long);
	  long networkInt;
	  char* bp = (char *) &networkInt;
  
	  while (bytesLeft) {
		int bytesRecv = recv(inSock, bp, bytesLeft, 0);
		if (bytesRecv <= 0){
		  // Failed to receive bytes
		  cerr << "Failed to receive bytes. Closing socket: " << inSock << "." << endl;
		  return -1;
		}
		bytesLeft = bytesLeft - bytesRecv;
		bp = bp + bytesRecv;
	  }
	  return ntohl(networkInt);
	}
	
	string GetMessage(int inSock, int messageLength) {

	  // Retrieve msg
	  int bytesLeft = messageLength;
	  char buffer[messageLength];
	  char* buffPTR = buffer;
	  while (bytesLeft > 0){
		int bytesRecv = recv(inSock, buffPTR, messageLength, 0);
		if (bytesRecv <= 0) {
		  // Failed to Read for some reason.
		  cerr << "Could not recv bytes. Closing socket: " << inSock << "." << endl;
		  return "";
		}
		bytesLeft = bytesLeft - bytesRecv;
		buffPTR = buffPTR + bytesRecv;
	  }

	  return buffer;
	}

	bool SendInteger(int outSock, int hostInt) {

	  // Local Variables
	  long networkInt = htonl(hostInt);

	  // Send Integer (as a long)
	  int didSend = send(outSock, &networkInt, sizeof(long), 0);
	  if (didSend != sizeof(long)){
		// Failed to Send
		cerr << "Unable to send data. Closing socket: " << outSock << "."  << endl;
		return false;
	  }

	  return true;
	}
	
	bool SendMessage(int outSock, string msg) {

	  // Local Variables
	  int msgLength = msg.length()+1;
	  char msgBuff[msgLength];
	  strcpy(msgBuff, msg.c_str());
	  msgBuff[msgLength-1] = '\0';

	  // Since they now know how many bytes to receive, we'll send the message
	  int msgSent = send(outSock, msgBuff, msgLength, 0);
	  if (msgSent != msgLength){
		// Failed to send
		cerr << "Unable to send data. Closing socket: " << outSock << "." << endl;
		return false;
	  }

	  return true;
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
}
#endif // defined _COMMON_H_