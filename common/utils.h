#ifndef __UTILS_H__
#define __UTILS_H__

#define error(STR, ...) \
	fprintf(stderr, STR "\n", ##__VA_ARGS__), \
	fflush(stderr), \
	exit(EXIT_FAILURE)

#define UNUSED(var) var __attribute__((unused))

#ifdef DEBUG
#define debug(STR, ...) \
	fprintf(stderr, STR "\n", ##__VA_ARGS__), \
	fflush(stderr)
#else
#define debug(...)
#endif

int parse_int(char**);

#endif
