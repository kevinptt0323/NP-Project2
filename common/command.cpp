#include "command.h"
#include <cstring>
#include <string>
#include <glob.h>

command::command() {
	file_in = file_out = "";
}

command::command(const char* _cmd) {
	char *cmd = new char[strlen(_cmd)+1];
	strcpy(cmd, _cmd);
	char *argv, *ptr;
	argv = strtok_r(cmd, " ", &ptr);
	do {
		if (strcmp(argv, "<") == 0)
			file_in = strtok_r(NULL, " ", &ptr);
		else if (strcmp(argv, ">") == 0)
			file_out = strtok_r(NULL, " ", &ptr);
		else {
			emplace_back(argv);
		}
	} while ((argv = strtok_r(NULL, " ", &ptr)));
	delete[] cmd;
}

command::command(const std::string& cmd) {
	command(cmd.c_str());
}
