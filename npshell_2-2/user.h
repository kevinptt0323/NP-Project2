#ifndef __USER_MANAGER_H__
#define __USER_MANAGER_H__

#include <cstring>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <fstream>

#include "number_pipe.h"

#define DEFAULT_NICKNAME "(no name)"

using std::string;
using std::vector;
using std::ostream;

class User {
public:
	sockaddr_in addr;
	int sockfd;
	string nickname;
	number_pipe number_pipe_manager;

	User();
	bool operator==(const User&) const;
	bool operator==(const int&) const;
	operator bool() const;
};

ostream& operator<<(ostream&, const sockaddr_in&);

class UserManager : public vector<User> {
public:
	using iterator = std::vector<User>::iterator;
	using const_iterator = std::vector<User>::const_iterator;
	void login(const sockaddr_in&, const int&, const string& =DEFAULT_NICKNAME);
	void login(const User&);
	void logout(const int&);
	void logout(const iterator&);
	UserManager::iterator find(const int&);
};

#endif
