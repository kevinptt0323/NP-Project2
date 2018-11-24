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

void UserManager::login(const sockaddr_in& addr, const int& sockfd, const string& nickname) {
	User user;
	user.addr = addr;
	user.sockfd = sockfd;
	user.nickname = nickname;
	login(user);
}

void UserManager::login(const User& user) {
	bool ok = false;
	for(auto& user_: *this) {
		if (!user_) {
			user_ = move(user);
			ok = true;
			break;
		}
	}
	if (!ok) {
		emplace_back(user);
	}
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
