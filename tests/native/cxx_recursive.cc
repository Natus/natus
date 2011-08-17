#include "test.hh"

int doTest (Value& global) {
	assert (!global.setRecursive ("step.step.value", 3L, Value::PropAttrNone, true).isException ());

	assert (!global.getRecursive ("step").isException ());
	assert (global.getRecursive ("step").isObject ());

	assert (!global.getRecursive ("step.step").isException ());
	assert (global.getRecursive ("step.step").isObject ());

	assert (!global.getRecursive ("step.step.value").isException ());
	assert (global.getRecursive ("step.step.value").isNumber ());
	assert (global.getRecursive ("step.step.value").to<int> () == 3);

	assert (!global.delRecursive ("step.step.value").isException ());
	assert (!global.getRecursive ("step.step").isException ());
	assert (global.getRecursive ("step.step").isObject ());
	assert (!global.getRecursive ("step").isException ());
	assert (global.getRecursive ("step").isObject ());

	assert (!global.delRecursive ("step.step").isException ());
	assert (!global.getRecursive ("step").isException ());
	assert (global.getRecursive ("step").isObject ());

	return 0;
}
