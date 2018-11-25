#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <vector>
#include <string>

class command : public std::vector<std::string> {
public:
	std::string file_in, file_out;
	int user_pipe_in, user_pipe_out;
	command();
	command(const char*);
	command(const std::string&);
};

#endif
