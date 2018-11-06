#include <cstdio>
#include <cstring>
#include <deque>
#include <algorithm>

#include <fcntl.h>
#include <signal.h>

#include "command.h"
#include "builtin.h"
#include "job.h"
#include "signal_handler.h"

#define BUF_SIZE 16000

using namespace std;

const char* SHELL_NAME = "npshell";
const char prompt[] = "% ";
char buf[BUF_SIZE+1];
deque<pair<int,Fd2>> fd_arr;

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

bool cmp_fd_pair(const pair<int,Fd2>& a, const pair<int,Fd2>& b) {
	return a.first < b.first;
}

/* Reduces counter of each item in fd_arr.
 */
void reduce_command_count() {
	for(auto& fd: fd_arr) {
		fd.first--;
	}
}

int main() {
	init_signal_handler();
	init_builtins();

	job("setenv PATH bin:.").exec();

	while (1) {
		print_prompt(prompt);
		if (input_command(buf, BUF_SIZE)) {
			if (strlen(buf) > 0) {
				reduce_command_count();
				job curr_job(buf);
				if (fd_arr.size() > 0 && fd_arr.front().first == 0) {
					curr_job.pipe_in = fd_arr.front().second;
					fd_arr.pop_front();
				}
				auto pipe_next_n_item = make_pair(curr_job.pipe_next_n, curr_job.pipe_out);
				bool same_pipe_next_n = false;
				if (curr_job.pipe_next_n > 0) {
					auto ptr = lower_bound(fd_arr.begin(), fd_arr.end(), pipe_next_n_item, cmp_fd_pair);
					if (ptr != fd_arr.end() && ptr->first == curr_job.pipe_next_n) {
						same_pipe_next_n = true;
						curr_job.pipe_out = ptr->second;
					}
				}
				curr_job.exec();
				if (curr_job.pipe_next_n > 0 && !same_pipe_next_n) {
					auto ptr = upper_bound(fd_arr.begin(), fd_arr.end(), pipe_next_n_item, cmp_fd_pair);
					fd_arr.emplace(ptr, curr_job.pipe_next_n, curr_job.pipe_out);
				}
			}
		} else {
			break;
		}
	}
	return 0;
}
