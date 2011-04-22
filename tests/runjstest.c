#include <unistd.h>
#include <limits.h>

#include "test.h"

int onEngine(const char *engine, int argc, const char **argv) {
	char cmd[PATH_MAX];
	strncpy(cmd, NATUSBIN " -e '", PATH_MAX);
	strncat(cmd, engine, PATH_MAX - strlen(cmd));
	strncat(cmd, "' ", PATH_MAX - strlen(cmd));
	if (argc > 1)
		strncat(cmd, argv[1], PATH_MAX - strlen(cmd));

	int es = system(cmd);
	return WEXITSTATUS(es);
}
