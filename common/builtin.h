#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "type.h"

using std::string;

typedef int (*HandlerFunc)(const char*, const char *const[], const char*, const FILENO&);

struct builtin {
	string name;
	HandlerFunc handler;
	string opts;

	builtin();
	builtin(const char*, HandlerFunc, const char*);
	~builtin();
	int exec(const char *const[], const FILENO& = DEFAULT_FILENO);
	int exec(const std::vector<string>&, const FILENO& = DEFAULT_FILENO);
};

extern std::unordered_map<string, builtin> builtins;
extern int shell_pid, shell_pgid;
extern bool exit_flag;

void init_builtins();

#endif
