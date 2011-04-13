#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <cassert>
#include <sys/types.h>
#include <dirent.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.hpp>
using namespace natus;

#include "../test.h"

int doTest(Engine& engine, Value& global);

int onEngine(const char *eng, int argc, const char **argv) {
	Engine engine;
	if (!engine.initialize(eng)) {
		fprintf(stderr, "Unable to init engine! %s\n", eng);
		return 1;
	}
	printf("Engine: %s\n", engine.getName());

	Value global = engine.newGlobal();
	return doTest(engine, global);
}
