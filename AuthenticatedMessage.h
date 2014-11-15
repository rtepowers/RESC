// AUTHOR: Ray Powers
// DATE: November 13, 2014

// DESCRIPTION: AuthenticatedMessage is an extension of the Message class by
//              composition. Will include authentication headers

#ifndef _AUTHENTICATED_MESSAGE_H_
#define _AUTHENTICATED_MESSAGE_H_

#include <iostream>
#include "Message.h"

using namespace std;

class AuthenticatedMessage
{
	public:
		AuthenticatedMessage();
		~AuthenticatedMessage();
	private:
		Message  *msgBody;
		string authToken;
};

#endif /* _AUTHENTICATED_MESSAGE_H_ */