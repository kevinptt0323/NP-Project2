#define __NPSHELL_MAJOR__ 2
#define __NPSHELL_MINOR__ 2

#include <cstdio>
#include <cstring>

#include <sstream>

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

/*
void remote_shell(int client_sockfd) {

	job("setenv PATH bin:.").exec();

	extern bool exit_flag;
	exit_flag = false;

	while (!exit_flag) {
		print_prompt(client_sockfd, prompt);
		if (input_command(client_sockfd, buf, BUF_SIZE)) {
			if (strlen(buf) > 0) {
				number_pipe_manager.reduce_count();
				job curr_job(buf);
				curr_job.set_stdin_fileno(client_sockfd);
				curr_job.set_stdout_fileno(client_sockfd);
				curr_job.set_stderr_fileno(client_sockfd);
				const Fd2* pipe_in = number_pipe_manager.get_fd2(0);
				if (pipe_in) {
					curr_job.pipe_in = move(*pipe_in);
					number_pipe_manager.pop_front();
				}
				bool same_pipe_next_n = false;
				if (curr_job.pipe_next_n > 0) {
					const Fd2* pipe_out = number_pipe_manager.get_fd2(curr_job.pipe_next_n);
					if (pipe_out) {
						same_pipe_next_n = true;
						curr_job.pipe_out = move(*pipe_out);
					}
				}
				curr_job.exec();
				if (curr_job.pipe_next_n > 0 && !same_pipe_next_n) {
					number_pipe_manager.set_fd2(curr_job.pipe_next_n, curr_job.pipe_out);
				}
			}
		} else {
			break;
		}
	}

	number_pipe_manager.close();
	close(client_sockfd);
}
*/

UserManager user_manager;

int bin_who(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	const Args& args
) {
	stringstream ss;
	ss << "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
	int idx = 0;
	const User& me = args.get<User>("user");
	for(const auto& user: user_manager) {
		++idx;
		if (user) {
			string indicate_me = user == me ? "<-me" : "";
			ss << idx << "\t" << user.nickname << "\t" << user.addr << "\t" << indicate_me << "\n";
		}
	}
	string s = ss.str();
	write(args.get<FILENO>("fileno").OUT, s.c_str(), s.length());
	return 0;
}

int bin_tell(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	const char *opts,
	const Args& args
) {
	User& me = args.get<User>("user");
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
	stringstream ss;
	ss << "*** " << me.nickname << " " << (yell ? "yelled" : "told you") << "***: " << message << endl;
	user_manager.broadcast(ss.str());
	return 0;
}

int bin_name(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	const Args& args
) {
	User& me = args.get<User>("user");
	me.nickname = argv[1];
	stringstream ss;
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

	int max_fd = sockfd;
	fd_set master, read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(sockfd, &master);

	extern bool exit_flag;
	exit_flag = false;

	while(1) {
		read_fds = master;
		if (select(max_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
		}
		for(int i=0; i<=max_fd; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == sockfd) {
					sockaddr_in client_addr;
					unsigned addrlen = sizeof(client_addr);
					int client_sockfd = accept(sockfd, (sockaddr*) &client_addr, &addrlen);
					FD_SET(client_sockfd, &master);
					max_fd = max(max_fd, client_sockfd);
					const User& user = user_manager.login(client_addr, client_sockfd);
					stringstream ss;
					ss << "*** User '" << user.nickname << "' entered from " << user.addr << " ***" << endl;
					user_manager.broadcast(ss.str());
					print_prompt(client_sockfd, prompt);
				} else {
					int client_sockfd = i;
					auto user = user_manager.find(client_sockfd);
					if (input_command(client_sockfd, buf, BUF_SIZE)) {
						if (strlen(buf) > 0) {
							user->number_pipe_manager.reduce_count();
							job curr_job(buf);
							curr_job.set_stdin_fileno(client_sockfd);
							curr_job.set_stdout_fileno(client_sockfd);
							curr_job.set_stderr_fileno(client_sockfd);
							const Fd2* pipe_in = user->number_pipe_manager.get_fd2(0);
							if (pipe_in) {
								curr_job.pipe_in = move(*pipe_in);
								user->number_pipe_manager.pop_front();
							}
							bool same_pipe_next_n = false;
							if (curr_job.pipe_next_n > 0) {
								const Fd2* pipe_out = user->number_pipe_manager.get_fd2(curr_job.pipe_next_n);
								if (pipe_out) {
									same_pipe_next_n = true;
									curr_job.pipe_out = move(*pipe_out);
								}
							}
							Args args;
							args["fileno"] = new FILENO(DEFAULT_FILENO);
							args["user"] = &*user;
							args["raw"] = &buf;
							curr_job.exec(args);
							if (curr_job.pipe_next_n > 0 && !same_pipe_next_n) {
								user->number_pipe_manager.set_fd2(curr_job.pipe_next_n, curr_job.pipe_out);
							}
						}
					} else {
						job("exit").exec();
					}
					if (exit_flag) {
						string nickname = user->nickname;
						user_manager.logout(user);
						close(client_sockfd);
						stringstream ss;
						ss << "*** User '" << nickname << "' left. ***" << endl;
						user_manager.broadcast(ss.str());
						FD_CLR(client_sockfd, &master);
						exit_flag = false;
					} else {
						print_prompt(client_sockfd, prompt);
					}
				}
			}
		}
	}

	return 0;
}
