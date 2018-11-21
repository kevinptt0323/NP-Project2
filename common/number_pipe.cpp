#include "number_pipe.h"

bool cmp_fd_pair(const pair<int,Fd2>& a, const int n) {
	return a.first < n;
}

void number_pipe::reduce_count() {
	for(auto& fd: *this) {
		fd.first--;
	}
}

const Fd2* number_pipe::get_fd2(int n) const {
	auto ptr = lower_bound(begin(), end(), n, cmp_fd_pair);
	return ptr != end() && ptr->first == n ? &ptr->second : NULL;
}

void number_pipe::set_fd2(int n, const Fd2& fd) {
	auto ptr = lower_bound(begin(), end(), n, cmp_fd_pair);
	if (ptr != end() && ptr->first == n) {
		ptr->second = move(fd);
	} else {
		emplace(ptr, n, fd);
	}
}
