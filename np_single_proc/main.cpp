#define __NPSHELL_MAJOR__ 2
#define __NPSHELL_MINOR__ 2

#include <cstdio>
#include <cstring>

#include <sstream>
#include <map>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "builtin.h"
#include "command.h"
#include "job.h"
#include "number_pipe.h"
#include "signal_handler.h"
#include "utils.h"

#include "user.h"

#define BUF_SIZE 16000

using namespace std;

const char* SHELL_NAME = "npshell";
const char prompt[] = "% ";
char buf[BUF_SIZE+1];

int start_tcp_server(const int port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1){
		fprintf(stderr, "Fail to create a socket.\n");
		exit(0);
	}

	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEPORT) failed");
#endif

	sockaddr_in server_addr;
	server_addr.sin_family = PF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
		fprintf(stderr, "Fail to bind to addr.\n");
	}

	if (listen(sockfd, 1) != 0) {
		fprintf(stderr, "Fail to listen to socket.\n");
	}
	return sockfd;
}

void print_prompt(int fd, const char* prompt) {
	write(fd, prompt, strlen(prompt));
}

char* input_command(int fd, char* buf, int bufsize) {
	int len = read(fd, buf, bufsize - 1);
	buf[len] = '\0';
	while (len && buf[len-1] < 32) {
		buf[--len] = '\0';
	}
	return buf;
}

map<pair<int,int>,Fd2> user_pipes;

Fd2* get_user_pipe(const User& user1, const User& user2) {
	auto key = make_pair(user1.id, user2.id);
	auto itr = user_pipes.find(key);
	if (itr != user_pipes.end()) {
		return &itr->second;
	}
	return NULL;
}

void set_user_pipe(const User& user1, const User& user2, const Fd2& fd) {
	auto key = make_pair(user1.id, user2.id);
	user_pipes[key] = fd;
}

void set_user_pipe(const User& user1, const User& user2) {
	auto key = make_pair(user1.id, user2.id);
	user_pipes.erase(key);
}

UserManager user_manager;

int bin_who(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	const Args& args
) {
	const User& me = args.get<User>("user");
	stringstream ss;
	ss << "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
	int idx = 0;
	for(const auto& user: user_manager) {
		++idx;
		if (user) {
			string indicate_me = user == me ? "<-me" : "";
			ss << idx << "\t" << user.nickname << "\t" << user.addr << "\t" << indicate_me << "\n";
		}
	}
	me.send(ss.str());
	return 0;
}

int bin_tell(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	const char *opts,
	const Args& args
) {
	const User& me = args.get<User>("user");
	char* raw = args.get<char[]>("raw");
	char* message;
	bool yell = strcmp(opts, "broadcast") == 0;
	for(int i=0, cnt=0; raw[i]; i++) {
		if (i > 0 && raw[i] == ' ' && raw[i-1] != ' ') {
			cnt++;
		}
		if (cnt == (yell ? 1 : 2) && raw[i] != ' ') {
			message = raw + i;
			break;
		}
	}
	string s = "*** " + me.nickname + " " + (yell ? "yelled" : "told you") + " ***: " + message + "\n";
	if (yell) {
		user_manager.broadcast(s);
	} else {
		int user_id = atoi(argv[1]);
		auto user_itr = user_manager.find_by_id(user_id);
		if (user_itr == user_manager.end()) {
			string error_s = string("*** Error: user #") + argv[1] + " does not exist yet. ***\n";
			me.send(error_s);
		} else {
			user_itr->send(s);
		}
	}
	return 0;
}

int bin_name(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	const Args& args
) {
	User& me = args.get<User>("user");
	stringstream ss;
	for(const auto& user: user_manager) {
		if (user && user.nickname == argv[1]) {
			ss << "*** User '" << argv[1] << "' already exists. ***" << endl;
			me.send(ss.str());
			return 1;
		}
	}
	me.nickname = argv[1];
	ss << "*** User from " << me.addr << " is named '" << me.nickname << "'. ***" << endl;
	user_manager.broadcast(ss.str());
	return 0;
}

int main(int argc, char* argv[]) {
	int port = 8787;
	if (argc == 2) port = atoi(argv[1]);
	int sockfd = start_tcp_server(port);
	init_signal_handler();
	init_builtins();

	add_builtin("who", bin_who, "");
	add_builtin("tell", bin_tell, "");
	add_builtin("yell", bin_tell, "broadcast");
	add_builtin("name", bin_name, "");

	int max_fd = 256;
	fd_set master, read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(sockfd, &master);

	extern bool exit_flag;
	exit_flag = false;

	while(1) {
		read_fds = master;
		if (select(max_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR) {
				continue;
			}
			error("select error");
		}
		for(int i=0; i<=max_fd; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == sockfd) {
					sockaddr_in client_addr;
					unsigned addrlen = sizeof(client_addr);
					int client_sockfd = accept(sockfd, (sockaddr*) &client_addr, &addrlen);
					FD_SET(client_sockfd, &master);
					max_fd = max(max_fd, client_sockfd);
					User& user = user_manager.login(client_addr, client_sockfd);
					job("clearenv").exec();
					job("setenv PATH bin:.").exec();
					user.getenv();
					stringstream ss;
					ss << "*** User '" << user.nickname << "' entered from " << user.addr << ". ***" << endl;
					user_manager.broadcast(ss.str());
					print_prompt(client_sockfd, prompt);
				} else {
					int client_sockfd = i;
					auto me_itr = user_manager.find_by_sockfd(client_sockfd);
					auto& me = *me_itr;
					me.setenv();
					if (input_command(client_sockfd, buf, BUF_SIZE)) {
						if (strlen(buf) > 0) {
							string error_s = "";
							me.number_pipe_manager.reduce_count();
							job curr_job(buf);
							curr_job.set_stdin_fileno(client_sockfd);
							curr_job.set_stdout_fileno(client_sockfd);
							curr_job.set_stderr_fileno(client_sockfd);
							int user_pipe_in = curr_job.front().user_pipe_in;
							int user_pipe_out = curr_job.back().user_pipe_out;
							if (~user_pipe_in) {
								auto pipe_user_itr = user_manager.find_by_id(user_pipe_in);
								if (pipe_user_itr != user_manager.end()) {
									Fd2* user_pipe = get_user_pipe(*pipe_user_itr, me);
									if (!user_pipe) {
										error_s = "*** Error: the pipe #" + \
										          to_string(pipe_user_itr->id) + \
										          "->#" + \
										          to_string(me.id) + \
										          " does not exist yet. ***\n";
									} else {
										curr_job.pipe_in = *user_pipe;
										set_user_pipe(*pipe_user_itr, me);
									}
								} else {
									error_s = "*** Error: user #" + to_string(user_pipe_in) + " does not exist yet. ***\n";
								}
							} else {
								const Fd2 *pipe_in = me.number_pipe_manager.get_fd2(0);
								if (pipe_in) {
									curr_job.pipe_in = *pipe_in;
									me.number_pipe_manager.pop_front();
								}
							}
							bool same_pipe_next_n = false;
							if (~user_pipe_out) {
								auto pipe_user_itr = user_manager.find_by_id(user_pipe_out);
								if (pipe_user_itr != user_manager.end()) {
									Fd2 *user_pipe = get_user_pipe(me, *pipe_user_itr);
									if (user_pipe) {
										error_s = "*** Error: the pipe #" + \
										          to_string(me.id) + \
										          "->#" + \
										          to_string(pipe_user_itr->id) + \
										          " already exists. ***\n";
									} else {
										user_pipe = new Fd2();
										pipe(user_pipe->data());
										set_user_pipe(me, *pipe_user_itr, *user_pipe);
										curr_job.pipe_out = *user_pipe;
										delete user_pipe;
									}
								} else {
									error_s = "*** Error: user #" + to_string(user_pipe_out) + " does not exist yet. ***\n";
								}
							} else {
								if (curr_job.pipe_next_n > 0) {
									const Fd2 *pipe_out = me.number_pipe_manager.get_fd2(curr_job.pipe_next_n);
									if (pipe_out) {
										same_pipe_next_n = true;
										curr_job.pipe_out = *pipe_out;
									}
								}
							}
							Args args;
							args["fileno"] = new FILENO(DEFAULT_FILENO);
							args["user"] = &me;
							args["raw"] = &buf;
							if (error_s == "") {
								curr_job.exec(args);
							} else {
								me.send(error_s);
							}
							if (curr_job.pipe_next_n > 0 && !same_pipe_next_n) {
								me.number_pipe_manager.set_fd2(curr_job.pipe_next_n, curr_job.pipe_out);
							}
						}
					} else {
						job("exit").exec();
					}
					if (exit_flag) {
						string nickname = me.nickname;
						user_manager.logout(me_itr);
						close(client_sockfd);
						string s = "*** User '" + nickname + "' left. ***\n";
						user_manager.broadcast(s);
						FD_CLR(client_sockfd, &master);
						exit_flag = false;
					} else {
						me.getenv();
						print_prompt(client_sockfd, prompt);
					}
				}
			}
		}
	}

	return 0;
}
