#include "test.hh"

int doTest(Engine& engine, Value& global) {
	Value exc = throwException(global, "Error", "Foo %d", 23);
	assert(exc.to<UTF8>() == "Error: Foo 23");

	exc = throwException(global, "Error", 7, "Foo %d", 23);
	assert(exc.to<UTF8>() == "Error: 7: Foo 23");
	return 0;
}
