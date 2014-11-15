// AUTHOR: Ray Powers
// DATE: November 13, 2014

// DESCRIPTION: Message class will handle Messages

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include<iostream>

using namespace std;

enum MessageType {
	INVALID_MSG,
	DIRECT_MSG
};

class Message {
public:
	Message(string msg, int toUser, int fromUser);
	~Message();
	
	// Help Methods
	string PrintMessage();
private:
	int _dataLength;
	char * _data;
	int _toUserId;
	int _fromUserId;
	MessageType messageType;
};

Message::Message(string msg, int toUser, int fromUser) 
{
	_dataLength = msg.length() + 1;
	_data = new char[_dataLength];
	strcpy(_data, msg.c_str());
	_data[_dataLength - 1] = '\0';
	
	_toUserId = toUser;
	_fromUserId = fromUser;
}

Message::~Message()
{
	delete [] _data;
}

string Message::PrintMessage()
{
	return string(_data);
}

#endif /* _MESSAGE_H_ */
