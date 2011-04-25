#include "test.hpp"
#include <natus/require.hpp>

static bool freecalled = false;
void hook_free(void* misc) {
	assert(misc == (void*) 0x1234);
	freecalled = true;
}

Value hook(Value& ctx, Require::HookStep step, char* name, void* misc) {
	if (step == Require::HookStepLoad && !strcmp(name, "__internal__")) {
		Value mod = ctx.newObject();
		assert(!mod.set("misc", (long) misc).isException());
		return mod;
	}

	return NULL;
}

int doTest(Engine& engine, Value& global) {
	Value config = global.newObject();
	Value   path = arrayBuilder(global, "./");
	config.setRecursive("natus.require.path", path, Value::PropAttrNone, true);

	// Initialize the require system
	Require req(global);
	assert(req.initialize(config));
	assert(req.addHook("__internal__", hook, (void*) 0x1234, hook_free));

	// Load the internal module
	Value modulea = req.require("__internal__");
	assert(!modulea.isException());
	assert(modulea.isObject());
	assert(modulea.get("misc").to<int>() == 0x1234);

	// Load the internal module again
	Value moduleb = req.require("__internal__");
	assert(!moduleb.isException());
	assert(moduleb.isObject());
	assert(moduleb.get("misc").to<int>() == 0x1234);

	// Check to make sure that the underlying values refer to the same object
	assert(!modulea.set("test", 17L).isException());
	assert(moduleb.get("test").to<int>() == 17);

	// Load a script file via standard spec
	Value scriptmod = req.require("scriptmod");
	assert(!scriptmod.isException());
	assert(scriptmod.isObject());
	assert(scriptmod.get("number").to<int>() == 115);
	assert(scriptmod.get("string").to<UTF8>() == "hello world");

	// Load a script file via relative spec
	scriptmod = req.require("./scriptmod");
	assert(!scriptmod.isException());
	assert(scriptmod.isObject());
	assert(scriptmod.get("number").to<int>() == 115);
	assert(scriptmod.get("string").to<UTF8>() == "hello world");

	// Cleanup
	return 0;
}
