#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <cassert>
#include <sys/types.h>
#include <dirent.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.hpp>
using namespace natus;

#define ENGINEDIR "../engines/.libs"

int doTest(Engine& engine, Value& global);

int main() {
	int retval = 0;

	DIR* dir = opendir(ENGINEDIR);
	if (!dir) return 1;

	struct dirent *ent = NULL;
	while ((ent = readdir(dir))) {
		if (retval != 0) break;

		size_t flen = strlen(ent->d_name);
		size_t slen = strlen(MODSUFFIX);

		if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name))     continue;
		if (flen < slen || strcmp(ent->d_name + flen - slen, MODSUFFIX)) continue;

		char *tmp = (char *) new char[flen + strlen(ENGINEDIR) + 2];
		if (!tmp) {
			retval = 1;
			break;
		}
		strcpy(tmp, ENGINEDIR);
		strcat(tmp, "/");
		strcat(tmp, ent->d_name);

		Engine engine;
		if (!engine.initialize(tmp)) {
			retval = 1;
			fprintf(stderr, "Unable to init engine! %s\n",tmp);
		}
		printf("Engine: %s\n", engine.getName());

		if (retval == 0) {
			Value global = engine.newGlobal();
			// Run each test 100 times to check for incremental memory corruption
			for (int i=0 ; i < 1 ; i++) {
				if ((retval = doTest(engine, global)))
					break;
			}
		}
		delete[] tmp;
	}

	closedir(dir);
	return retval;
}
