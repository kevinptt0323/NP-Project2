#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <signal.h>

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

void print_prompt(const char* prompt) {
	fputs(prompt, stdout);
	fflush(stdout);
}

char* input_command(char* buf, int bufsize) {
	char* ret = fgets(buf, bufsize, stdin);
	if (ret == NULL) {
		return NULL;
	}
	buf[strlen(buf)-1] = '\0';
	return buf;
}

int main() {
	init_signal_handler();
	init_builtins();
	number_pipe number_pipe_manager;

	job("setenv PATH bin:.").exec();

	extern bool exit_flag;
	exit_flag = false;

	while (!exit_flag) {
		print_prompt(prompt);
		if (input_command(buf, BUF_SIZE)) {
			if (strlen(buf) > 0) {
				number_pipe_manager.reduce_count();
				job curr_job(buf);
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
	return 0;
}
