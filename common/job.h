#ifndef __JOB_H__
#define __JOB_H__

#include <array>
#include <string>
#include <vector>
#include <fcntl.h>

#include "command.h"

using std::string;
using std::vector;

typedef std::array<int,2> Fd2;

pid_t create_process(const command&, int, int, const vector<Fd2>&);

class job : public vector<command> {
public:
	vector<pid_t> pids;
	pid_t pgid;
	string cmd;
	int pipe_next_n;
	bool pipe_next_err;
	Fd2 pipe_in, pipe_out, pipe_err;

	job();
	job(const char*);
	int exec();
};

#endif
