#ifndef __TYPE_H__
#define __TYPE_H__

#include <array>
#include <unistd.h>

typedef std::array<int,2> Fd2;

struct FILENO {
	int IN, OUT, ERR;
};

#define DEFAULT_FILENO { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO }

#endif
