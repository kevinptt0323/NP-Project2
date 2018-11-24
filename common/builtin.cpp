#include <string>
#include <unordered_map>
#include <vector>
#include <signal.h>

#include "builtin.h"
#include "utils.h"

using std::string;

std::unordered_map<string, builtin> builtins;
bool exit_flag;

builtin::builtin() {
	handler = NULL;
}

builtin::builtin(const char *_name, HandlerFunc _handler, const char *_opts) {
	name = _name;
	handler = _handler;
	opts = _opts;
}

builtin::~builtin() { }

int builtin::exec(const std::vector<string>& argv0, const Args& args) {
	const char** argv = new const char*[argv0.size()+1];
	for(size_t i=0; i<argv0.size(); i++) {
		argv[i] = argv0[i].c_str();
	}
	argv[argv0.size()] = NULL;

	int ret = exec(argv, args);
	delete[] argv;
	return ret;
}

int builtin::exec(const char *const argv[], const Args& args) {
	return handler(name.c_str(), argv, opts.c_str(), args);
}

int bin_break(
	UNUSED(const char *name),
	UNUSED(const char *const argv[]),
	UNUSED(const char *opts),
	UNUSED(const Args& args)
) {
	// exit(0);
	exit_flag = true;
	return 0;
}

int bin_setenv(
	UNUSED(const char *name),
	const char *const argv[],
	UNUSED(const char *opts),
	UNUSED(const Args& args)
) {
	const char* key = argv[1];
	const char* val = argv[2];
	if (key == NULL || val == NULL) {
		return -1;
	}
	return setenv(key, val, 1);
}

int bin_printenv(
	UNUSED(const char *name),
	const char *const argv[],
	UNUSED(const char *opts),
	const Args& args
) {
	const char* key = argv[1];
	if (key == NULL) {
		return -1;
	}
	const char* env = getenv(key);
	if (env == NULL) {
		return -1;
	}
	const FILENO& fileno = args.get<FILENO>("fileno");
	write(fileno.OUT, env, strlen(env));
	write(fileno.OUT, "\n", 1);
	return 0;
}

int bin_unsetenv(
	UNUSED(const char *name),
	const char *const argv[],
	UNUSED(const char *opts),
	UNUSED(const Args& args)
) {
	const char* key = argv[1];
	return unsetenv(key);
}

void init_builtins() {
	add_builtin("exit", bin_break, "");
	add_builtin("setenv", bin_setenv, "");
	add_builtin("printenv", bin_printenv, "");
	add_builtin("unsetenv", bin_unsetenv, "");
}

