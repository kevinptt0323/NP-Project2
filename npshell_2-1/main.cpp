#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "command.h"
#include "builtin.h"
#include "job.h"
#include "signal_handler.h"
#include "number_pipe.h"

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
	if (buf[len-1] == '\n') {
		buf[len-1] = '\0';
	}
	return buf;
}

void remote_shell(int client_sockfd) {
	number_pipe number_pipe_manager;
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
}

int main(int argc, char* argv[]) {
	int port = 8787;
	if (argc == 2) port = atoi(argv[1]);
	int sockfd = start_tcp_server(port);
	init_signal_handler();
	init_builtins();

	while(1) {
		sockaddr_in client_addr;
		unsigned addrlen;
		int client_sockfd = accept(sockfd, (sockaddr*) &client_addr, &addrlen);
		char ip_address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(client_addr.sin_addr), ip_address, INET_ADDRSTRLEN);
		fprintf(stderr, "connected from %s:%u\n", ip_address, client_addr.sin_port);
		remote_shell(client_sockfd);
	}

	return 0;
}
