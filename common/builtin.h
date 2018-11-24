#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "type.h"
#include "utils.h"

using std::string;

#ifndef add_builtin
#define add_builtin(name, ...) \
	builtins[name] = builtin(name, __VA_ARGS__)
#endif

typedef int (*HandlerFunc)(const char*, const char *const[], const char*, const Args&);

struct builtin {
	string name;
	HandlerFunc handler;
	string opts;

	builtin();
	builtin(const char*, HandlerFunc, const char*);
	~builtin();
	int exec(const char *const[], const Args& = Args());
	int exec(const std::vector<string>&, const Args& = Args());
};

extern std::unordered_map<string, builtin> builtins;
extern int shell_pid, shell_pgid;
extern bool exit_flag;

void init_builtins();

#endif
