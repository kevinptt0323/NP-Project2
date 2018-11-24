#ifndef __TYPE_H__
#define __TYPE_H__

#include <array>
#include <string>
#include <unordered_map>
#include <unistd.h>

using std::string;
using std::unordered_map;

typedef std::array<int,2> Fd2;

struct FILENO {
	int IN, OUT, ERR;
};

class Args : public unordered_map<string,void*> {
public:
	template<typename T>
	T& get(const string& key) const {
		const auto& itr = find(key);
		return *static_cast<T*>(itr->second);
	}
};

const FILENO DEFAULT_FILENO = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };

#endif
