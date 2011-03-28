#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	Value x = fromJSON(global, "{\"a\": 17, \"b\": 2.1}");
	assert(!x.isException());
	assert(x.isObject());
	assert(x.get("a").toLong() == 17);
	assert(x.get("b").toDouble() == 2.1);
	assert(toJSON(x).toStringUTF8() == "{\"a\":17,\"b\":2.1}");
	return 0;
}
