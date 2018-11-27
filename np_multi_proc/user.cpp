#include <fstream>
#include <arpa/inet.h>
#include "user.h"

using std::ostream;

User::User() : id(-1), sockfd(-1), parent(NULL) { }

bool User::operator==(const User& rhs) const {
	return id == rhs.id;
	// return addr.sin_addr.s_addr == rhs.addr.sin_addr.s_addr && addr.sin_port == rhs.addr.sin_port;
}

bool User::operator!=(const User& rhs) const {
	return !(*this == rhs);
}

User::operator bool() const {
	return id != -1;
}

void User::send(const char* s, const int& length) const {
	if (*this) {
		write(sockfd, s, length);
	}
}

void User::send(const char* s) const {
	send(s, strlen(s));
}

void User::send(const string& s) const {
	send(s.c_str(), s.length());
}

void User::getenv() {
	environ.clear();
	for(char** ptr = ::environ; *ptr; ptr++) {
		string env = *ptr;
		auto pos = env.find("=");
		string key = env.substr(0, pos);
		string val = env.substr(pos + 1);
		environ.emplace_back(key, val);
	}
}

void User::setenv() const {
	clearenv();
	for(const auto& env: environ) {
		::setenv(env.first.c_str(), env.second.c_str(), 1);
	}
}

ostream& operator<<(ostream& out, const sockaddr_in& addr) {
	char ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr.sin_addr), ip_address, INET_ADDRSTRLEN);
	out << ip_address << "/" << addr.sin_port;
	// out << "CGILAB/511";
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
	if (user.id != -1) {
		if (find_by_id(user.id) != end()) {
			// fprintf(stderr, "login failed\n");
		} else {
			if ((int)size() < user.id) {
				resize(user.id);
			}
			this->operator[](user.id-1) = user;
		}
		return this->operator[](user.id-1);
	}
	int idx = 0;
	for(User& user_: *this) {
		idx++;
		if (!user_) {
			user_ = user;
			user_.id = idx;
			return user_;
			break;
		}
	}
	User& user_ = emplace_back(user);
	user_.id = idx + 1;
	return user_;
}

void UserManager::logout(const int& i) {
	logout(begin() + i);
}

void UserManager::logout(const iterator& itr) {
	fprintf(stderr, "logout %lu\n", itr->number_pipe_manager.size());
	itr->number_pipe_manager.close();
	itr->id = -1;
	itr->sockfd = -1;
}

UserManager::iterator UserManager::find_by_id(const int& id) {
	if ((int)size() >= id && this->operator[](id-1)) {
		return begin() + id - 1;
	} else {
		return end();
	}
}

UserManager::iterator UserManager::find_by_sockfd(const int& sockfd) {
	for(UserManager::iterator itr=begin(); itr!=end(); itr++) {
		if (itr->sockfd == sockfd) {
			return itr;
		}
	}
	return end();
}

void UserManager::broadcast(const char* s, const int& length) const {
	// write(1, s, length);
	for(auto& user: *this) {
		user.send(s, length);
	}
}

void UserManager::broadcast(const char* s) const {
	broadcast(s, strlen(s));
}

void UserManager::broadcast(const string& s) const {
	broadcast(s.c_str(), s.length());
}
