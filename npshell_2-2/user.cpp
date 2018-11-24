#include <fstream>
#include <arpa/inet.h>
#include "user.h"

using std::ostream;

User::User() : sockfd(-1) { }

bool User::operator==(const User& rhs) const {
	return addr.sin_addr.s_addr == rhs.addr.sin_addr.s_addr && addr.sin_port == rhs.addr.sin_port;
}
bool User::operator==(const int& fd) const {
	return sockfd == fd;
}

User::operator bool() const {
	return sockfd != -1;
}

ostream& operator<<(ostream& out, const sockaddr_in& addr) {
	char ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr.sin_addr), ip_address, INET_ADDRSTRLEN);
	out << ip_address << "/" << addr.sin_port;
	return out;
}

User& UserManager::login(const sockaddr_in& addr, const int& sockfd, const string& nickname) {
	User user;
	user.addr = addr;
	user.sockfd = sockfd;
	user.nickname = nickname;
	return login(user);
}

User& UserManager::login(const User& user) {
	bool ok = false;
	for(auto& user_: *this) {
		if (!user_) {
			user_ = user;
			ok = true;
			return user_;
			break;
		}
	}
	// assert(!ok);
	return emplace_back(user);
}

void UserManager::logout(const int& i) {
	logout(begin() + i);
}

void UserManager::logout(const iterator& itr) {
	itr->number_pipe_manager.close();
	itr->sockfd = -1;
}

UserManager::iterator UserManager::find(const int& sockfd) {
	for(UserManager::iterator itr=begin(); itr!=end(); itr++) {
		if (itr->sockfd == sockfd) {
			return itr;
		}
	}
	return end();
}

void UserManager::broadcast(const char* s) {
	broadcast(s, strlen(s));
}

void UserManager::broadcast(const char* s, const int& length) {
	write(1, s, length);
	for(auto& user: *this) {
		if (user) {
			write(user.sockfd, s, length);
		}
	}
}

void UserManager::broadcast(const string& s) {
	broadcast(s.c_str(), s.length());
}
