#define __NPSHELL_MAJOR__ 2
#define __NPSHELL_MINOR__ 3

#include <cstdio>
#include <cstring>

#include <sstream>
#include <map>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

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

UserManager user_manager;

string user_pipe_name(const User& user1, const User& user2) {
	return "user_pipe/fifo_" + to_string(user1.id) + "_" + to_string(user2.id);
}

bool check_user_pipe(const User& user1, const User& user2) {
	string fifoname = user_pipe_name(user1, user2);
	return access( fifoname.c_str(), F_OK ) != -1;
}

int open_user_pipe(const User& user1, const User& user2, int flag) {
	string fifoname = user_pipe_name(user1, user2);
	// mkfifo(fifoname.c_str(), 0644);
	int fd = open(fifoname.c_str(), flag | O_CREAT, 0644);
	return fd;
}

void clear_user_pipe(const User& user) {
	for(const auto& user2: user_manager) {
		unlink(user_pipe_name(user, user2).c_str());
		unlink(user_pipe_name(user2, user).c_str());
	}
}

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
			string indicate_me = user == me ? "\t<-me" : "";
			ss << idx << "\t" << user.nickname << "\t" << user.addr << indicate_me << "\n";
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
	string s = to_string(me.id) + " " + argv[0];
	if (!yell) {
		s += " "s + argv[1];
	}
	s += " \""s + message + "\"";
	if (yell) {
		me.send("*** " + me.nickname + " yelled ***: " + message + "\n");
		me.parent->send(s);
	} else {
		int user_id = atoi(argv[1]);
		auto user_itr = user_manager.find_by_id(user_id);
		if (user_itr == user_manager.end()) {
			string error_s = string("*** Error: user #") + argv[1] + " does not exist yet. ***\n";
			me.send(error_s);
		} else {
			me.parent->send(s);
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
	for(const auto& user: user_manager) {
		if (user && user.nickname == argv[1]) {
			me.send("*** User '"s + argv[1] + "' already exists. ***\n");
			return 1;
		}
	}
	me.nickname = argv[1];
	stringstream ss;
	ss << "*** User from " << me.addr << " is named '" << me.nickname << "'. ***" << endl;
	me.send(ss.str());
	me.parent->send(to_string(me.id) + " name \"" + me.nickname + "\"");
	return 0;
}

int bin_break_2(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	const Args& args
) {
	User& me = args.get<User>("user");
	me.send("*** User '" + me.nickname + "' left. ***\n");
	exit_flag = true;
	if (me.parent) {
		me.parent->send(to_string(me.id) + " exit");
	}
	return 0;
}

pid_t login(User& me) {
	job("clearenv").exec();
	job("setenv PATH bin:.").exec();
	me.getenv();
	me.send("****************************************\n"
	"** Welcome to the information server. **\n"
	"****************************************\n");
	stringstream ss;
	ss << "*** User '" << me.nickname << "' entered from " << me.addr << ". ***" << endl;
	me.send(ss.str());

	pid_t pid;
	while ((pid = fork()) < 0) {
		debug("Fork error: %s", strerror(errno));
		usleep(100);
	}
	return pid;
}

void start_remote_shell(int me_id, const int& to_child) {
	auto me_itr = user_manager.find_by_id(me_id);
	int max_fd = max(me_itr->sockfd, to_child);
	fd_set master, read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(me_itr->sockfd, &master);
	FD_SET(to_child, &master);

	extern bool exit_flag;
	exit_flag = false;

	print_prompt(me_itr->sockfd, prompt);

	while (!exit_flag) {
		read_fds = master;
		if (select(max_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR) {
				continue;
			}
			error("select error");
		}
		int curr_fd = -1;
		for(int i=0; i<=max_fd; i++) {
			if (FD_ISSET(i, &read_fds)) {
				curr_fd = i;
				break;
			}
		}
		User& me = *me_itr;
		if (curr_fd == me.sockfd) {
			me.setenv();
			if (input_command(me.sockfd, buf, BUF_SIZE)) {
				if (strlen(buf) > 0) {
					fprintf(stderr, "child%d: %s\n", me.id, buf);
					string error_s = "";
					me.number_pipe_manager.reduce_count();
					job curr_job(buf);
					curr_job.set_stdin_fileno(me.sockfd);
					curr_job.set_stdout_fileno(me.sockfd);
					curr_job.set_stderr_fileno(me.sockfd);
					int user_pipe_in = curr_job.front().user_pipe_in;
					int user_pipe_out = curr_job.back().user_pipe_out;
					if (~user_pipe_in) {
						auto pipe_user_itr = user_manager.find_by_id(user_pipe_in);
						if (pipe_user_itr != user_manager.end()) {
							if (!check_user_pipe(*pipe_user_itr, me)) {
								error_s = "*** Error: the pipe #" + \
											to_string(pipe_user_itr->id) + \
											"->#" + \
											to_string(me.id) + \
											" does not exist yet. ***\n";
							} else {
								curr_job.pipe_in = {open_user_pipe(*pipe_user_itr, me, O_RDONLY), -1};
								unlink(user_pipe_name(*pipe_user_itr, me).c_str());
								string s = "*** " + \
											me.nickname + " (#" + to_string(me.id) + \
											") just received from " + \
											pipe_user_itr->nickname + " (#" + to_string(pipe_user_itr->id) + \
											") by '" + buf + "' ***\n";
								me.send(s);
								me.parent->send(to_string(me.id) + " read " + to_string(pipe_user_itr->id) + " \"" + buf + "\"");
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
							if (check_user_pipe(me, *pipe_user_itr)) {
								error_s = "*** Error: the pipe #" + \
											to_string(me.id) + \
											"->#" + \
											to_string(pipe_user_itr->id) + \
											" already exists. ***\n";
							} else {
								curr_job.pipe_out = {-1, open_user_pipe(me, *pipe_user_itr, O_WRONLY)};
								curr_job.pipe_user = true;
								string s = "*** " + \
											me.nickname + " (#" + to_string(me.id) + \
											") just piped '" + buf + "' to " + \
											pipe_user_itr->nickname + " (#" + to_string(pipe_user_itr->id) + \
											") ***\n";
								me.send(s);
								me.parent->send(to_string(me.id) + " write " + to_string(pipe_user_itr->id) + " \"" + buf + "\"");
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
			me.getenv();
			if (exit_flag) {
				exit_flag = false;
			} else {
				print_prompt(me.sockfd, prompt);
			}
		} else if (curr_fd == to_child) {
			if (input_command(curr_fd, buf, BUF_SIZE)) {
				fprintf(stderr, "parent->%d: %s\n", me.id, buf);
				int user_id;
				string cmd;
				{
					char cmd_[1024];
					sscanf(buf, "%d %s", &user_id, cmd_);
					cmd = cmd_;
				}
				auto user_itr = user_manager.find_by_id(user_id);
				if (cmd == "login") {
					if (user_id != me.id) {
						char nickname[32];
						User user;
						user.id = user_id;
						sscanf(buf, "%*d %*s %d \"%31[^\"]\" %u %hu", &user.sockfd, nickname, &user.addr.sin_addr.s_addr, &user.addr.sin_port);
						user.nickname = nickname;
						user_manager.login(user);
						me_itr = user_manager.find_by_id(me_id);
						stringstream ss;
						ss << "*** User '" << user.nickname << "' entered from " << user.addr << ". ***" << endl;
						me_itr->send(ss.str());
					}
				} else if (cmd == "name") {
					char nickname[32];
					sscanf(buf, "%*d %*s \"%31[^\"]\"", nickname);
					if (*user_itr != me) {
						user_itr->nickname = nickname;
						stringstream ss;
						ss << "*** User from " << user_itr->addr << " is named '" << user_itr->nickname << "'. ***" << endl;
						me.send(ss.str());
					}
				} else if (cmd == "exit") {
					if (me == *user_itr) {
						clear_user_pipe(me);
						exit_flag = true;
					} else {
						me_itr->send("*** User '" + user_itr->nickname + "' left. ***\n");
					}
					user_manager.logout(user_itr);
				} else if (cmd == "yell") {
					if (*user_itr != me) {
						char message[1024];
						sscanf(buf, "%*d %*s \"%1023[^\"]\"", message);
						me_itr->send("*** " + user_itr->nickname + " yelled ***: " + message + "\n");
					}
				} else if (cmd == "tell") {
					char message[1024];
					sscanf(buf, "%*d %*s %*d \"%1023[^\"]\"", message);
					me_itr->send("*** " + user_itr->nickname + " told you ***: " + message + "\n");
				} else if (cmd == "write") {
					if (*user_itr != me) {
						char cmd[1024];
						int u1_id, u2_id;
						sscanf(buf, "%d %*s %d \"%1023[^\"]\"", &u1_id, &u2_id, cmd);
						User &u1 = *user_manager.find_by_id(u1_id), &u2 = *user_manager.find_by_id(u2_id);
						string s = "*** " + \
									u1.nickname + " (#" + to_string(u1.id) + \
									") just piped '" + cmd + "' to " + \
									u2.nickname + " (#" + to_string(u2.id) + \
									") ***\n";
						me_itr->send(s);
					}
				} else if (cmd == "read") {
					if (*user_itr != me) {
						char cmd[1024];
						int u1_id, u2_id;
						sscanf(buf, "%d %*s %d \"%1023[^\"]\"", &u1_id, &u2_id, cmd);
						User &u1 = *user_manager.find_by_id(u1_id), &u2 = *user_manager.find_by_id(u2_id);
						string s = "*** " + \
									u1.nickname + " (#" + to_string(u1.id) + \
									") just received from " + \
									u2.nickname + " (#" + to_string(u2.id) + \
									") by '" + cmd + "' ***\n";
						me_itr->send(s);
					}
				}
			}
		} else {
		}
	}
}

int main(int argc, char* argv[]) {
	int port = 8787;
	if (argc == 2) port = atoi(argv[1]);
	int server_sockfd = start_tcp_server(port);
	init_signal_handler();
	init_builtins();

	add_builtin("who", bin_who, "");
	add_builtin("tell", bin_tell, "");
	add_builtin("yell", bin_tell, "broadcast");
	add_builtin("name", bin_name, "");
	add_builtin("exit", bin_break_2, "");

	int max_fd = server_sockfd;
	fd_set master, read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(server_sockfd, &master);

	while(1) {
		read_fds = master;
		if (select(max_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR) {
				continue;
			}
			error("select error");
		}
		int curr_fd = -1;
		for(int i=0; i<=max_fd; i++) {
			if (FD_ISSET(i, &read_fds)) {
				curr_fd = i;
				break;
			}
		}
		if (curr_fd == server_sockfd) {
			sockaddr_in client_addr;
			unsigned addrlen = sizeof(client_addr);
			int client_sockfd = accept(server_sockfd, (sockaddr*) &client_addr, &addrlen);
			User& me = user_manager.login(client_addr, client_sockfd);
			Fd2 to_parent, to_child;
			pipe(to_parent.data());
			pipe(to_child.data());
			pid_t child_pid = login(me);
			if (child_pid < 0) {
				error("login error");
			} else if (child_pid != 0) {
				fprintf(stderr, "parent: child_pid = %d\n", child_pid);
				close(me.sockfd);

				FD_SET(to_parent[0], &master);
				max_fd = max(max_fd, to_parent[0]);
				me.sockfd = to_child[1];
				close(to_child[0]);
				close(to_parent[1]);
				stringstream ss;
				ss << me.id << " login " << me.sockfd << " \"" << me.nickname << "\" " << me.addr.sin_addr.s_addr << " " << me.addr.sin_port;
				user_manager.broadcast(ss.str());
			} else {
				char ip_address[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(client_addr.sin_addr), ip_address, INET_ADDRSTRLEN);
				fprintf(stderr, "child%d: connected from %s:%u\n", me.id, ip_address, client_addr.sin_port);
				// me.in_child = true;
				me.parent = new User();
				me.parent->id = 0;
				me.parent->sockfd = to_parent[1];
				start_remote_shell(me.id, to_child[0]);
				// string s = "*** User '" + me.nickname + "' left. ***\n";
				User& me = user_manager[client_sockfd];
				close(me.parent->sockfd);
				FD_CLR(me.parent->sockfd, &master);
				delete me.parent;
				close(to_child[0]);
				close(me.sockfd);
				auto me_itr = user_manager.find_by_sockfd(me.sockfd);
				user_manager.logout(me_itr);
				break;
			}
		} else {
			if (input_command(curr_fd, buf, BUF_SIZE)) {
				int user_id;
				string cmd;
				{
					char cmd_[1024];
					sscanf(buf, "%d %s", &user_id, cmd_);
					cmd = cmd_;
				}
				auto user_itr = user_manager.find_by_id(user_id);
				if (user_itr == user_manager.end()) {
					error("%s: invalid sender %d", cmd.c_str(), user_id);
				} else {
					if (cmd == "name") {
						char nickname[322];
						sscanf(buf, "%*d %*s \"%31[^\"]\"", nickname);
						user_itr->nickname = nickname;
						user_manager.broadcast(buf);
					} else if (cmd == "exit") {
						user_manager.broadcast(buf);
						close(user_itr->sockfd);
						user_manager.logout(user_itr);
						close(curr_fd);
						FD_CLR(curr_fd, &master);
					} else if (cmd == "yell") {
						user_manager.broadcast(buf);
					} else if (cmd == "tell") {
						int user2_id;
						sscanf(buf, "%*d %*s %d", &user2_id);
						auto user2_itr = user_manager.find_by_id(user2_id);
						if (user2_itr == user_manager.end()) {
							error("tell: invalid receiver %d", user2_id);
						} else {
							user2_itr->send(buf);
						}
					} else if (cmd == "write" || cmd == "read") {
						user_manager.broadcast(buf);
					}
				}
			}
		}
	}

	return 0;
}
