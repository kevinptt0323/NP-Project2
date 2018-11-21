#include "utils.h"

int parse_int(char** str_ptr) {
	int ret = 0;
	for(; **str_ptr >= '0' && **str_ptr <= '9'; ++*str_ptr)
		ret = ret * 10 + **str_ptr - '0';
	return ret;
}
