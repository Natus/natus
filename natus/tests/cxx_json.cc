#include "test.hpp"

int doTest(Engine& engine, Value& global) {
	Value x = fromJSON(global, "{\"a\": 17, \"b\": 2.1}");
	assert(!x.isException());
	assert(x.isObject());
	assert(x.get("a").to<int>() == 17);
	assert(x.get("b").to<double>() == 2.1);
	assert(toJSON(x).to<UTF8>() == "{\"a\":17,\"b\":2.1}");
	return 0;
}
