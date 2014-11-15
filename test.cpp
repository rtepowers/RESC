

#include<iostream>
#include<string>

#include "Message.h"

using namespace std;

int main(int argc, char * argv[]) {
	string tmp = "HEllo World!";
	Message *msg = new Message(tmp);
	cout << "The message is: " << msg -> PrintMessage() << endl;

	return 0;
}