#include "test.hh"

int doTest (Value& global) {
	assert (!global.set ("x", global.newNumber (3)).isException ());
	assert (3 == global.get ("x").to<int> ());
	assert (3.0 == global.get ("x").to<double> ());
	assert (!global.set ("x", global.newNumber (3.1)).isException ());
	assert (3 == global.get ("x").to<int> ());
	assert (3.1 == global.get ("x").to<double> ());

	// Cleanup
	assert (!global.del ("x").isException ());
	assert (global.get ("x").isUndefined ());
	return 0;
}
