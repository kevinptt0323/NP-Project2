#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <sys/wait.h>
#include <sys/errno.h>
#include <unistd.h>
#include "builtin.h"
#include "job.h"
#include "utils.h"

using std::vector;

const char delims[] = "|!";

job::job() :
	IN_FILENO(STDIN_FILENO),
	OUT_FILENO(STDOUT_FILENO),
	ERR_FILENO(STDERR_FILENO),
	pipe_next_n(0),
	pipe_next_err(false),
	pipe_in({IN_FILENO, -1}),
	pipe_out({-1, OUT_FILENO}),
	pipe_err({-1, ERR_FILENO}) {
}

job::job(const char* cmd_in) : job() {
	cmd = cmd_in;

	char* cmd_ = new char[strlen(cmd_in) + 1];
	strcpy(cmd_, cmd_in);

	char *cmd_sep, *ptr1;

	cmd_sep = strtok_r(cmd_, delims, &ptr1);
	do {
		emplace_back(cmd_sep);
		if (ptr1 && ptr1[0] >= '0' && ptr1[0] <= '9') {
			const char delim = ptr1[0] ? cmd_in[ptr1 - cmd_ - 1] : '\0';
			pipe_next_n = parse_int(&ptr1);
			pipe_next_err = delim == '!';
		}
	} while ((cmd_sep = strtok_r(NULL, delims, &ptr1)));
	delete[] cmd_;
}

void job::set_stdin_fileno(int fd) {
	pipe_in[0] = IN_FILENO = fd;
}

void job::set_stdout_fileno(int fd) {
	pipe_out[1] = OUT_FILENO = fd;
}

void job::set_stderr_fileno(int fd) {
	pipe_err[1] = ERR_FILENO = fd;
}

pid_t create_process(const command& argv0, int IN_FILENO, int OUT_FILENO, int ERR_FILENO, int in_fd, int out_fd, int err_fd) {
	pid_t pid;
	while ((pid = fork()) < 0) {
		debug("Fork error: %s", strerror(errno));
		usleep(100);
	}
	if (pid == 0) {
		if (argv0.redirect_in != "") {
			int fd = open(argv0.redirect_in.c_str(), O_RDONLY);
			if (fd == -1) {
				error("%s: %s", strerror(errno), argv0.redirect_in.c_str());
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		} else if (in_fd != STDIN_FILENO) {
			dup2(in_fd, STDIN_FILENO);
		}
		if (argv0.redirect_out != "") {
			int fd = open(argv0.redirect_out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd == -1) {
				error("%s: %s", strerror(errno), argv0.redirect_out.c_str());
			}
			dup2(fd, STDOUT_FILENO);
			close(fd);
		} else if (out_fd != STDOUT_FILENO) {
			dup2(out_fd, STDOUT_FILENO);
		}
		if (err_fd != STDERR_FILENO) {
			dup2(err_fd, STDERR_FILENO);
		}

		if (in_fd != IN_FILENO) {
			close(in_fd);
		}
		if (out_fd != OUT_FILENO) {
			close(out_fd);
		}
		if (err_fd != ERR_FILENO) {
			close(err_fd);
		}

		char** argv = new char*[argv0.size()+1];
		for(size_t i=0; i<argv0.size(); i++) {
			argv[i] = (char*)argv0[i].c_str();
		}
		argv[argv0.size()] = NULL;

		if (execvp(argv[0], argv) == -1) {
			error("Unknown command: [%s].", argv[0]);
		}
		delete[] argv;
	} else {
		return pid;
	}
	return 0;
}

int job::exec(Args args) {
	if (size() == 0) return 1;
	vector<Fd2> pipes(size()-1);

	extern std::unordered_map<string, builtin> builtins;
	if (pipe_in[1] > 0) {
		close(pipe_in[1]);
	}
	for(size_t i=0; i<size(); i++) {
		if (i < pipes.size()) {
			while (pipe(pipes[i].data()) == -1) {
				debug("Cannot create pipe: %s", strerror(errno));
				usleep(100);
			}
		} else if (pipe_next_n > 0 && pipe_out[1] == OUT_FILENO) {
			// Create pipe to forward message to the next n-th line
			while (pipe(pipe_out.data()) == -1) {
				debug("Cannot create pipe: %s", strerror(errno));
				usleep(100);
			}
		}

		Fd2 pipe_in_i = i == 0 ? pipe_in : pipes[i-1];
		Fd2 pipe_out_i = i == size()-1 ? pipe_out : pipes[i];
		Fd2 pipe_err_i = i == size()-1 && pipe_next_err ? pipe_out_i : pipe_err;
		const auto &argv = operator[](i);
		if (builtins.find(argv[0]) != builtins.end()) {
			args["fileno"] = new FILENO({IN_FILENO, OUT_FILENO, ERR_FILENO});
			builtins[argv[0]].exec(argv, args);
			delete static_cast<FILENO*>(args["fileno"]);
			args.erase("fileno");
			pid_t pid = 0;
			pids.emplace_back(pid);
		} else {
			pid_t pid = create_process(argv, IN_FILENO, OUT_FILENO, ERR_FILENO, pipe_in_i[0], pipe_out_i[1], pipe_err_i[1]);
			pids.emplace_back(pid);
			if (i == 0)
				pgid = pid;
			setpgid(pid, pgid);
		}
		if (pipe_in_i[0] != IN_FILENO) {
			close(pipe_in_i[0]);
		}
		if (pipe_out_i[1] != OUT_FILENO) {
			if (pipe_next_n == 0 || i != size()-1) {
				close(pipe_out_i[1]);
			}
		}
	}

	if (pipe_next_n == 0) {
		for(pid_t job_pid: pids) {
			int status;
			if (!job_pid) continue;
			waitpid(job_pid, &status, 0);
		}
	}

	return 0;
}
