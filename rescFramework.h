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

	enum RESCCommand {
		RESCClientMessage,
		RESCServerMessage,
		RESCTrackerMessage
	};

	struct RESCHeader {
		RESCCommand cmd;
		char dest[19]; // 9bytes for userID, 1byte for SENTINEL, and 9bytes for ServerID
		char source[19]; // Same as dest
	};

	struct RESCMessage {
		RESCHeader hdr;
		char msg[256];
	};
	
	struct RESCServer {
		char id[9];
		char publicHostName[128];
	};
	
	struct RESCClient {
		char id[9];
		char username[32];
		char password[32];
	};

// Message Helper Functions
RESCMessage CreateMessage(RESCCommand msgType, string to, string from, string message)
{
	RESCHeader header;
	header.cmd = msgType;
	strcpy(header.dest, to.c_str());
	strcpy(header.source, from.c_str());
	RESCMessage newRescMessage;
	newRescMessage.hdr = header;
	strcpy(newRescMessage.msg, message.c_str());
	
	return newRescMessage;
}

// Network Helper Functions
	RESCMessage ReadRequest(int incomingSocket)
	{
		// Retrieve msg
		RESCMessage tmp;
		int RESCMessageSize = sizeof(RESCMessage);
		int bytesLeft = RESCMessageSize;
		char* buffer = new char[bytesLeft];
		char* buffPTR = buffer;
		while (bytesLeft > 0){
			int bytesRecvd = recv(incomingSocket, buffPTR, RESCMessageSize, 0);
			if (bytesRecvd <= 0) {
				// Failed to Read for some reason.
				cerr << "Could not recv bytes. Closing socket: " << incomingSocket << "." << endl;
				memcpy(&tmp, buffer, RESCMessageSize);
				return tmp;
			}
			bytesLeft = bytesLeft - bytesRecvd;
			buffPTR = buffPTR + bytesRecvd;
		}
		memcpy(&tmp, buffer, RESCMessageSize);
		return tmp;
		//return reinterpret_cast<RESCMessage*>(buffer);
	}
	
	bool SendRequest(int outgoingSocket, RESCMessage msg)
	{
		// SendMessage
		int msgLength = sizeof(RESCMessage);
		char msgBuff[msgLength];
		for (short i = 0; i < msgLength; i++) {
			msgBuff[i] = ((char*)&msg)[i];
		}

		// Since they now know how many bytes to receive, we'll send the message
		int msgSent = send(outgoingSocket, msgBuff, msgLength, 0);
		if (msgSent != msgLength){
			// Failed to send
			cerr << "Unable to send data. Closing socket: " << outgoingSocket << "." << endl;
			return false;
		}

		return true;
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
}
#endif // _RESCFRAMEWORK_H_