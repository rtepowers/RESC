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
	INVALID_MSG,
	BROADCAST_MSG,
	DIRECT_MSG,
	AUTH_MSG,
	AUTH_RESP_MSG,
	USERLIST_MSG
};

struct MsgHeader {
	int toUserId;
	int fromUserId;
	MsgType msgType;
};

struct Message {
	MsgHeader hdr;
	string body;
};

struct RESCUser {
	int id;
	string username;
};

// Framework Helper functions
Message CreateMessage(MsgType type, int to, int from, string input) 
{
	Message msg; 
	msg.hdr.toUserId = to;
	msg.hdr.fromUserId = from;
	msg.hdr.msgType = type;
	msg.body = input;
	
	return msg;
}
	
Message CreateAuthResponse(int userId, bool wasSuccessful)
{
	Message msg;
	msg.hdr.msgType = AUTH_RESP_MSG;
	msg.hdr.toUserId = userId;
	msg.body = (wasSuccessful)? "SUCCESSFUL" : "UNAUTHORIZED";
	return msg;
}

bool CheckAuthResponse(Message msg) 
{
	if (msg.hdr.msgType == INVALID_MSG) return false;
	return (!msg.body.compare("SUCCESSFUL"));
}

// Network Helper Functions
	bool SendHeader(int outSocket, MsgHeader hdr) {
		int hdrLength = sizeof(MsgHeader);
		char * hdrBuff = (char*)&hdr;
		int hdrSent = send(outSocket, hdrBuff, hdrLength, 0);
		if (hdrSent != hdrLength) {
			cerr << "Unable to send MsgHeader on socket: " << outSocket << endl;
			return false;
	 	}
		return true;
	}
	
	MsgHeader GetHeader(int inSocket) {
		MsgHeader hdr;
		int hdrLength = sizeof(MsgHeader);
		int bytesLeft = hdrLength;
		char* buffer = new char[bytesLeft];
		char* buffPtr = buffer;
		while (bytesLeft > 0) {
			int bytesRecvd = recv(inSocket, buffPtr, hdrLength, 0);
			if (bytesRecvd <= 0) {
				cerr << "Could not receive MsgHeader on socket: " << inSocket << endl;
				hdr.msgType = INVALID_MSG;
				delete [] buffer;
				return hdr;
			}
			bytesLeft -= bytesRecvd;
			buffPtr += bytesRecvd;
		}
		memcpy(&hdr, buffer, hdrLength);
		delete [] buffer;
		return hdr;
	}
	
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
	
	void SendMessage(int outSocket, Message msg) {
		if (msg.hdr.msgType == INVALID_MSG) {
			// Drop this Message;
			return;
		} else {
			SendHeader(outSocket, msg.hdr);
			if (!SendInteger(outSocket, msg.body.length()+1)) {
				cerr << "Unable to send Int. " << endl;
				return;
			}
			if (!SendData(outSocket, msg.body)) {
				cerr << "Unable to send Message. " << endl;
				return;
			}
		}
	}
	
	Message ReadMessage(int inSocket) {
		Message msg;
		msg.hdr = GetHeader(inSocket);
		long msgLength = GetInteger(inSocket);
		if (msgLength <= 0) {
			cerr << "Couldn't get integer." << endl;
			msg.hdr.msgType = INVALID_MSG;
		}
		string bodyMsg = GetData(inSocket, msgLength);
		if (bodyMsg == "") {
			cerr << "Couldn't get message." << endl;
			msg.hdr.msgType = INVALID_MSG;
		}
		msg.body = bodyMsg;
		
		return msg;
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