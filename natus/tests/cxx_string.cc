#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	assert(!global.set("x", global.newString("hello")).isException());
	assert(global.get("x").isString());
	assert(global.get("x").toStringUTF8() == "hello");

	// Cleanup
	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}
