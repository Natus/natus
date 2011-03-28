#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	assert(!global.set("x", global.newNumber(3)).isException());
	assert(3   == global.get("x").toLong());
	assert(3.0 == global.get("x").toDouble());
	assert(!global.set("x", global.newNumber(3.1)).isException());
	assert(3   == global.get("x").toLong());
	assert(3.1 == global.get("x").toDouble());

	// Cleanup
	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}
