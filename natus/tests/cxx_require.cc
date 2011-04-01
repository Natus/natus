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
	Value   path = global.newArrayBuilder(".");
	config.setRecursive("natus.require.path", path, Value::PropAttrNone, true);

	Require req(global);
	assert(req.initialize(config));

	assert(req.addHook("__internal__", hook, (void*) 0x1234, hook_free));
	Value module = req.require("__internal__");
	assert(!module.isException());
	assert(module.isObject());
	assert(module.get("misc").toLong() == 0x1234);

	// Cleanup
	return 0;
}
