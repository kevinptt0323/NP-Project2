#ifndef __USER_MANAGER_H__
#define __USER_MANAGER_H__

#include <cstring>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <fstream>

#include "number_pipe.h"
#include "type.h"

#define DEFAULT_NICKNAME "(no name)"

using std::string;
using std::vector;
using std::ostream;

class User {
public:
	vector<pair<string,string>> environ;
	int id;
	sockaddr_in addr;
	int sockfd;
	User* parent;
	string nickname;
	number_pipe number_pipe_manager;
	bool in_child;

	User();
	bool operator==(const User&) const;
	bool operator!=(const User&) const;
	operator bool() const;
	void send(const char*, const int&) const;
	void send(const char*) const;
	void send(const string&) const;
	void getenv();
	void setenv() const;
};

ostream& operator<<(ostream&, const sockaddr_in&);

class UserManager : public vector<User> {
public:
	using iterator = std::vector<User>::iterator;
	using const_iterator = std::vector<User>::const_iterator;
	User& login(const sockaddr_in&, const int&, const string& =DEFAULT_NICKNAME);
	User& login(const User&);
	void logout(const int&);
	void logout(const iterator&);
	UserManager::iterator find_by_id(const int&);
	UserManager::iterator find_by_sockfd(const int&);
	void broadcast(const char*, const int&) const;
	void broadcast(const char*) const;
	void broadcast(const string&) const;
};

#endif
