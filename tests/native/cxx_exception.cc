#include "test.hh"

int doTest (Engine& engine, Value& global) {
	Value exc = throwException (global, NULL, "Error", "Foo %d", 23);
	assert (exc.to<UTF8> () == "Error: Foo 23");

	exc = throwException (global, NULL, "Error", 7, "Foo %d", 23);
	assert (exc.to<UTF8> () == "Error: Foo 23");
	assert (exc.get("code").to<int>() == 7);
	return 0;
}
