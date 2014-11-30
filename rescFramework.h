// AUTHOR: Ray Powers
// DATE: October 25, 2014
// FILE: rescFramework.h

// DESCRIPTION: RESCFramework will replace Common.h. It will define protocols
//			  	and universal types to be used in the RESC system

#ifndef _RESCFRAMEWORK_H_
#define _RESCFRAMEWORK_H_

// Standard Library
#include<iostream>
#include<cstdio>
#include<string>

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

enum MsgType {
	INVALID_MSG = 0,
	DIRECT_MSG,
	BROADCAST_MSG,
	FILE_STREAM_MSG,
	USER_LIST_MSG
};

struct Message {
	MsgType cmd;
	string to;
	string from;
	string msg;
};

struct User {
	string username;
	string password;
	bool isConnected;
};

// Framework Helper functions
bool CheckAuthResponse(string msg) 
{
	return (!msg.compare("SUCCESSFUL"));
}

bool HasQuit(string msg)
{
	bool hasQuit = false;
	if (msg == "/quit" || msg == "/close" || msg == "/exit"){
		hasQuit = true;
	}
	return hasQuit;
}

Message ConvertMessage(string msg, string from)
{
	// Turn
	// "/msg userX blahblahblah"
	// into
	// ("blahblahblah", "userX", "", DIRECT_MSG)
	Message newMsg;
	newMsg.msg = "";
	newMsg.to = "";
	newMsg.from = from;
	newMsg.cmd = INVALID_MSG;
	string cmdName = "";
	
	const char * cMsg = msg.c_str();
	if (cMsg[0] == '/') {
		// Message was a command
		int cmdSize = 0;
		for (int i = 1; i < msg.length(); i++) {
			if (cMsg[i] == ' ') {
				cmdSize = i;
				break;
			}
		}
		if (cmdSize == 0) {
			// Direct Command (ie. No Args)
			cmdSize = msg.length();
		}
		// Build Command type
		stringstream ss;
		for (int i = 0; i < cmdSize; i++) {
			ss << cMsg[i];
		}
		cmdName.append(ss.str());
		ss.str("");
		ss.clear();
		
		// Now to interpret arg-based commands.
		if (cmdName == "/msg") {
			int userSize = 0;
			for (int i = cmdSize+1; i < msg.length(); i++) {
				if (cMsg[i] == ' ') {
					userSize = i;
					break;
				}
			}
			if (userSize == 0) {
				// No message, just action.
				userSize = msg.length();
			}
			// Build UserTo
			for (int i = cmdSize+1; i < userSize; i++) {
				ss << cMsg[i];
			}
			newMsg.to.append(ss.str());
			ss.str("");
			ss.clear();
			newMsg.cmd = DIRECT_MSG;
			
			for (int i = userSize+1; i < msg.length();i++) {
				ss << cMsg[i];
			}
			newMsg.msg.append(ss.str());
			ss.str("");
			ss.clear();
		} else if (cmdName == "/all") {
			for (int i = cmdSize+1; i < msg.length();i++) {
				ss << cMsg[i];
			}
			newMsg.cmd = BROADCAST_MSG;
			newMsg.msg.append(ss.str());
			ss.str("");
			ss.clear();
		}
	} else {
		newMsg.cmd = BROADCAST_MSG;
		newMsg.msg = msg;
	}
	
	return newMsg;
}

Message ConvertServerMessage(string rawMsg)
{
	// Turn
	// "/msg userX blahblahblah"
	// into
	// ("blahblahblah", "userX", "", DIRECT_MSG)
	Message newMsg;
	newMsg.msg = "";
	newMsg.to = "";
	newMsg.from = "SERVER";
	newMsg.cmd = INVALID_MSG;
	string cmdName = "";
	
	const char * cMsg = rawMsg.c_str();
	if (cMsg[0] == '/') {
		// Message was a command
		int cmdSize = 0;
		for (int i = 1; i < rawMsg.length(); i++) {
			if (cMsg[i] == ' ') {
				cmdSize = i;
				break;
			}
		}
		if (cmdSize == 0) {
			// Direct Command (ie. No Args)
			cmdSize = rawMsg.length();
		}
		// Build Command type
		stringstream ss;
		for (int i = 0; i < cmdSize; i++) {
			ss << cMsg[i];
		}
		cmdName.append(ss.str());
		ss.str("");
		ss.clear();
		
		// Now to interpret arg-based commands.
		if (cmdName == "/userlist") {
			for (int i = cmdSize+1; i < rawMsg.length();i++) {
				ss << cMsg[i];
			}
			newMsg.msg.append(ss.str());
			newMsg.cmd = USER_LIST_MSG;
			ss.str("");
			ss.clear();
		} else if (cmdName == "/filestream") {
// 			for (int i = cmdSize+1; i < rawMsg.length();i++) {
// 				ss << cMsg[i];
// 			}
// 			newMsg.msg.append(ss.str());
// 			newMsg.cmd = FILE_STREAM_MSG;
// 			ss.str("");
// 			ss.clear();
			
			int userSize = 0;
			for (int i = cmdSize+1; i < rawMsg.length(); i++) {
				if (cMsg[i] == ' ') {
					userSize = i;
					break;
				}
			}
			if (userSize > 0) {
				// Build UserFrom
				for (int i = cmdSize+1; i < userSize; i++) {
					ss << cMsg[i];
				}
				newMsg.from = ss.str();
				ss.str("");
				ss.clear();
				newMsg.cmd = FILE_STREAM_MSG;
			
				for (int i = userSize+1; i < rawMsg.length();i++) {
					ss << cMsg[i];
				}
				newMsg.msg.append(ss.str());
				ss.str("");
				ss.clear();
			}
		}
	} else {
		newMsg.cmd = BROADCAST_MSG;
		newMsg.msg = rawMsg;
	}
	
	return newMsg;
}

// Network Helper Functions	
	bool SendData(int outSocket, string msg) {
	  // Local Variables
	  int msgLength = msg.length()+1;
	  char msgBuff[msgLength];
	  strcpy(msgBuff, msg.c_str());
	  msgBuff[msgLength-1] = '\0';

	  // Since they now know how many bytes to receive, we'll send the message
	  int msgSent = send(outSocket, msgBuff, msgLength, 0);
	  if (msgSent != msgLength){
		// Failed to send
		cerr << "Unable to send data. Closing clientSocket: " << outSocket << "." << endl;
		return false;
	  }

	  return true;
	}

	string GetData(int inSock, int messageLength) {
	  // Retrieve msg
	  int bytesLeft = messageLength;
	  char buffer[messageLength];
	  char* buffPTR = buffer;
	  while (bytesLeft > 0){
		int bytesRecv = recv(inSock, buffPTR, messageLength, 0);
		if (bytesRecv <= 0) {
		  // Failed to Read for some reason.
		  cerr << "Could not recv bytes. Closing clientSocket: " << inSock << "." << endl;
		  return "";
		}
		bytesLeft = bytesLeft - bytesRecv;
		buffPTR = buffPTR + bytesRecv;
	  }

	  return buffer;
	}

	long GetInteger(int inSock) {

	  // Retreive length of msg
	  int bytesLeft = sizeof(long);
	  long networkInt;
	  char* bp = (char *) &networkInt;

	  while (bytesLeft) {
		int bytesRecv = recv(inSock, bp, bytesLeft, 0);
		if (bytesRecv <= 0){
		  // Failed to receive bytes
		  cerr << "Failed to receive bytes. Closing clientSocket: " << inSock << "." << endl;
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
	
	void SendMessage(int outSocket, string msg) {
		if (!SendInteger(outSocket, msg.length()+1)) {
			cerr << "Unable to send Int. " << endl;
			return;
		}
		if (!SendData(outSocket, msg)) {
			cerr << "Unable to send Message. " << endl;
			return;
		}
	}
	
	string ReadMessage(int inSocket) {
		long msgLength = GetInteger(inSocket);
		if (msgLength <= 0) {
			cerr << "Couldn't get integer." << endl;
		}
		string bodyMsg = GetData(inSocket, msgLength);
		if (bodyMsg == "") {
			cerr << "Couldn't get message." << endl;
		}

		return bodyMsg;
	}
	
	
	int OpenSocket (string hostName, unsigned short serverPort) {
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
	
	void CloseSocket(int sock)
	{
		close(sock);
	}
}
#endif // _RESCFRAMEWORK_H_