#include "test.hpp"
#include <natus/require.hpp>

static bool freecalled = false;
void hook_free(void* misc) {
	assert(misc == (void*) 0x1234);
	freecalled = true;
}

Value hook(Value& ctx, Require::HookStep step, char* name, void* misc) {
	if (step == Require::HookStepLoad && !strcmp(name, "__internal__")) {
		assert(!ctx.setRecursive("exports.misc", (long) misc).isException());
		return ctx.newBool(true);
	}

	return NULL;
}

int doTest(Engine& engine, Value& global) {
	Value config = global.newObject();
	Value   path = global.newArrayBuilder("./lib");
	config.setRecursive("natus.require.path", path, Value::PropAttrNone, true);

	// Initialize the require system
	Require req(global);
	assert(req.initialize(config));
	assert(req.addHook("__internal__", hook, (void*) 0x1234, hook_free));

	// Load the internal module
	Value modulea = req.require("__internal__");
	assert(!modulea.isException());
	assert(modulea.isObject());
	assert(modulea.get("misc").toLong() == 0x1234);

	// Load the internal module again
	Value moduleb = req.require("__internal__");
	assert(!moduleb.isException());
	assert(moduleb.isObject());
	assert(moduleb.get("misc").toLong() == 0x1234);

	// Check to make sure that the underlying values refer to the same object
	assert(!modulea.set("test", 17L).isException());
	assert(moduleb.get("test").toLong() == 17);

	// Load a script file
	Value scriptmod = req.require("script");
	assert(!scriptmod.isException());
	assert(scriptmod.isObject());
	assert(scriptmod.get("number").toLong() == 115);
	assert(scriptmod.get("string").toStringUTF8() == "hello world");


	// Cleanup
	return 0;
}