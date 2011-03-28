#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	Value json = global.get("JSON");
	assert(global.isGlobal());
	assert(!json.isException());
	assert(json.isObject());
	assert(!json.isGlobal());

	// Test shared globals
	assert(!global.set("array",  global.newArray()).isException());
	assert(!global.set("bool",   global.newBool(true)).isException());
	assert(!global.set("nill",   global.newNull()).isException());
	assert(!global.set("number", global.newNumber(15)).isException());
	assert(!global.set("object", global.newObject()).isException());
	assert(!global.set("string", global.newString("hi")).isException());

	Value glb = engine.newGlobal(global);
	assert(glb.isGlobal());
	assert(!glb.set("array",  global.get("array")).isException());
	assert(!glb.set("bool",   global.get("bool")).isException());
	assert(!glb.set("nill",   global.get("nill")).isException());
	assert(!glb.set("number", global.get("number")).isException());
	assert(!glb.set("object", global.get("object")).isException());
	assert(!glb.set("string", global.get("string")).isException());

	assert(glb.get("array").get("length").toLong() == 0);
	assert(glb.get("bool").toBool());
	assert(glb.get("nill").isNull());
	assert(glb.get("number").toLong());
	assert(glb.get("object").isObject());
	assert(glb.get("string").toStringUTF8() == "hi");

	return 0;
}