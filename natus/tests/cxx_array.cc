#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	Value array = global.newArrayBuilder(global.newNumber(123)).newArrayBuilder(global.newNumber(456));
	assert(array.isArray());
	assert(array.get("length").toLong() == 2);
	assert(!global.set("x", array).isException());

	array = global.get("x");
	assert(array.isArray());
	assert(2 == array.get("length").toLong());
	assert(3 == array.call("push", global.newArray().newArrayBuilder(global.newString("foo"))).toLong());

	assert(123   == array.get(0).toLong());
	assert(456   == array.get(1).toLong());
	assert(array.get(2).toStringUTF8() == "foo");

	Value v = array.call("pop");
	assert(123   == array.get(0).toLong());
	assert(456   == array.get(1).toLong());
	assert(v.isString());
	assert(v.toStringUTF8() == "foo");

	assert(!array.set(0, v.newNumber(789)).isException());
	assert(789   == array.get(0).toLong());
	assert(456   == array.get(1).toLong());

	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}
