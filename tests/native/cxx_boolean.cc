#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	// Test true
	assert(!global.set("x", global.newBool(true)).isException());
	assert(global.get("x").to<bool>());

	// Test false
	assert(!global.set("x", global.newBool(false)).isException());
	assert(!global.get("x").to<bool>());

	// Cleanup
	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}