#include "test.hpp"
#include <string.h>

static Value firstarg_function(Value& fnc, Value& ths, Value& arg) {
	assert(ths.isGlobal() || ths.isUndefined());
	assert(fnc.isFunction());
	assert(arg.isArray());
	assert(arg.get("length").to<int>() > 0);
	return arg.get(0);
}

static Value exception_function(Value& fnc, Value& ths, Value& arg) {
	return firstarg_function(fnc, ths, arg).toException();
}

int doTest(Engine& engine, Value& global) {
	printf("%d\n", __LINE__);
	//global.newObject();
	printf("%d\n", __LINE__);
	//global.newObject();
	printf("%d\n", __LINE__);

	Value glbl = engine.newGlobal(global);
	Value rslt;
	Value func = global.newFunction(firstarg_function);
	assert(func.isFunction());
	assert(!global.set("x", func).isException());
	assert(!glbl.set("x", global.get("x")).isException());
	assert(global.get("x").isFunction());
	assert(glbl.get("x").isFunction());

	// Call from C++
	printf("%d\n", __LINE__);
	rslt = global.call("x", global.newArrayBuilder(global.newNumber(123)));
	assert(!rslt.isException());
	assert(123 == rslt.to<int>());
	rslt = glbl.call("x", glbl.newArrayBuilder(glbl.newNumber(123)));
	assert(!rslt.isException());
	assert(123 == rslt.to<int>());

	// New from C++
	printf("%d\n", __LINE__);
	rslt = global.callNew("x", global.newArrayBuilder(global.newObject()));
	assert(!rslt.isException());
	assert(rslt.isObject());
	rslt = glbl.callNew("x", glbl.newArrayBuilder(glbl.newObject()));
	assert(!rslt.isException());
	assert(rslt.isObject());

	// Call from JS
	printf("%d\n", __LINE__);
	rslt = global.evaluate("x(123);");
	assert(!rslt.isException());
	assert(123 == rslt.to<int>());
	rslt = glbl.evaluate("x(123);");
	assert(!rslt.isException());
	assert(123 == rslt.to<int>());

	// New from JS
	rslt = global.evaluate("new x({});");
	assert(!rslt.isException());
	assert(rslt.isObject());
	rslt = glbl.evaluate("new x({});");
	assert(!rslt.isException());
	assert(rslt.isObject());

	assert(!global.del("x").isException());
	assert(!glbl.del("x").isException());
	assert(global.get("x").isUndefined());
	assert(glbl.get("x").isUndefined());
	assert(!global.set("x", global.newFunction(exception_function)).isException());
	assert(!glbl.set("x", global.get("x")).isException());


	// Exception Call from C++
	rslt = global.call("x", global.newArrayBuilder(global.newNumber(123)));
	assert(rslt.isException());
	rslt = glbl.call("x", glbl.newArrayBuilder(glbl.newNumber(123)));
	assert(rslt.isException());

	// Exception New from C++
	printf("%d\n", __LINE__);
	rslt = global.callNew("x", global.newArrayBuilder(global.newObject()));
	printf("%d\n", __LINE__);
	assert(rslt.isException());
	rslt = glbl.callNew("x", glbl.newArrayBuilder(glbl.newObject()));
	assert(rslt.isException());

	// Exception Call from JS
	rslt = global.evaluate("x(123);");
	assert(rslt.isException());
	rslt = glbl.evaluate("x(123);");
	assert(rslt.isException());

	// Exception New from JS
	rslt = global.evaluate("new x({});");
	assert(rslt.isException());
	rslt = glbl.evaluate("new x({});");
	assert(rslt.isException());

	// TODO: OOM

	// Cleanup
	assert(!global.del("x").isException());
	assert(!glbl.del("x").isException());
	assert(global.get("x").isUndefined());
	assert(glbl.get("x").isUndefined());
	return 0;
}
