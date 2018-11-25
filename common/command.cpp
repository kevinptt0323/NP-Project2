#include "command.h"
#include <cstring>
#include <string>
#include <glob.h>

command::command() : file_in(""), file_out(""), user_pipe_in(-1), user_pipe_out(-1) {
}

command::command(const char* _cmd) : command() {
	char *cmd = new char[strlen(_cmd)+1];
	strcpy(cmd, _cmd);
	char *argv, *ptr;
	argv = strtok_r(cmd, " ", &ptr);
	do {
		if (strcmp(argv, "<") == 0)
			file_in = strtok_r(NULL, " ", &ptr);
		else if (strcmp(argv, ">") == 0)
			file_out = strtok_r(NULL, " ", &ptr);
		else if (argv[0] == '<')
			user_pipe_in = atoi(argv+1);
		else if (argv[0] == '>')
			user_pipe_out = atoi(argv+1);
		else
			emplace_back(argv);
	} while ((argv = strtok_r(NULL, " ", &ptr)));
	delete[] cmd;
}

command::command(const std::string& cmd) : command(cmd.c_str()) {
}
