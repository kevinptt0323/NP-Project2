#include <sys/wait.h>
#include <signal.h>

void sigchld_handler(int sig __attribute__((unused))) {
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		/* do nonthing */
	}
}

void init_signal_handler() {
	signal(SIGCHLD, sigchld_handler);
}
