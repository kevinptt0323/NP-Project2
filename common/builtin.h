#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;

typedef int (*HandlerFunc)(const char*, const char *const[], const char*);

struct builtin {
	string name;
	HandlerFunc handler;
	string opts;

	builtin();
	builtin(const char*, HandlerFunc, const char*);
	~builtin();
	int exec(const char *const[]);
	int exec(const std::vector<string>&);
};

extern std::unordered_map<string, builtin> builtins;
extern int shell_pid, shell_pgid;

void init_builtins();

#endif
