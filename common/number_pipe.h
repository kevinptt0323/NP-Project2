#ifndef __NUMBER_PIPE_H__
#define __NUMBER_PIPE_H__

#include <deque>
#include <algorithm>
#include "job.h"

using namespace std;

class number_pipe : public deque<pair<int,Fd2>> {
public:
	void reduce_count();
	const Fd2* get_fd2(int) const;
	void set_fd2(int, const Fd2&);
};

#endif

