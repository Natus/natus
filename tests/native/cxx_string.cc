#include "test.hh"

int doTest (Value& global) {
	assert (!global.set ("x", global.newString ("hello")).isException ());
	assert (global.get ("x").isString ());
	assert (global.get ("x").to<UTF8> () == "hello");

	// Cleanup
	assert (!global.del ("x").isException ());
	assert (global.get ("x").isUndefined ());
	return 0;
}
