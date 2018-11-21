#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <vector>
#include <string>

class command : public std::vector<std::string> {
public:
	std::string redirect_in, redirect_out;
	command();
	command(const char*);
	command(const std::string&);
};

#endif
