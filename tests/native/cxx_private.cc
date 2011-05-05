#include "test.hh"
#include <natus/private.h>

static int deleted = 0;

static void incDeleted(void *x) {
	if (!x) return;
	deleted++;
}

static void foreach(const char *name, void *priv, bool *up) {
	*up = priv != (void *) 0x1234;
}

static Value doNothing(Value& fnc, Value& ths, Value& arg) {
	return fnc.newBoolean(true);
}

static void testInternal(Value& global, const char* js, Value::Type type) {
	// Test private on an internal function
	global.evaluate(js);
	Value x = global.get("x");
	assert(x.isType(type));
	assert(!x.setPrivateName("test", (void *) 0x1234));
	assert(NULL == x.getPrivateName<void*>("test"));
	assert(!global.del("x").isException());
}

static void testExternal(Value& global, Value val, Value::Type type, bool works=false) {
	assert(!global.set("x", val).isException());
	Value x = global.get("x");
	assert(x.isType(type));
	if (works) {
		assert(x.setPrivateName("test", (void *) 0x1234));
		assert(0x1234 == x.getPrivateName<long>("test"));
	} else {
		assert(!x.setPrivateName("test", (void *) 0x1234));
		assert(NULL == x.getPrivateName<void*>("test"));
	}
	assert(!global.del("x").isException());
}

int doTest(Engine& engine, Value& global) {
	// Ensure we can store private on the global
	assert(global.setPrivateName("test", (void *) 0x1234));
	assert(0x1234 == global.getPrivateName<long>("test"));

	// Test private on an externally defined types
	testExternal(global, global.newArray(),             Value::TypeArray);
	testExternal(global, global.newBoolean(true),       Value::TypeBool);
	testExternal(global, global.newFunction(doNothing), Value::TypeFunction, true);
	testExternal(global, global.newNull(),              Value::TypeNull);
	testExternal(global, global.newNumber(1234),        Value::TypeNumber);
	testExternal(global, global.newObject(),            Value::TypeObject,   true);
	testExternal(global, global.newString("hi"),        Value::TypeString);
	testExternal(global, global.newUndefined(),         Value::TypeUndefined);

	// Test private on an internally defined types
	testInternal(global, "x = [];",            Value::TypeArray);
	testInternal(global, "x = true;",          Value::TypeBool);
	testInternal(global, "x = function() {};", Value::TypeFunction);
	testInternal(global, "x = null;",          Value::TypeNull);
	testInternal(global, "x = 1234;",          Value::TypeNumber);
	testInternal(global, "x = {};",            Value::TypeObject);
	testInternal(global, "x = 'hello';",       Value::TypeString);
	testInternal(global, "",                   Value::TypeUndefined);

	// Test the private datastructure
	ntPrivate *priv = nt_private_init();
	assert(priv);

	// Test set and clear
	deleted = 0;
	assert(nt_private_set(priv, "foo", (void *) 0x1234, incDeleted)); // Set
	assert(nt_private_set(priv, "foo", NULL, NULL));  // Delete
	assert(deleted == 1);

	// Test push
	assert(nt_private_push(priv, (void *) 0x1234, incDeleted));
	assert(nt_private_push(priv, (void *) 0x4321, incDeleted));

	// Test iteration
	bool up = true;
	nt_private_foreach(priv, false, (ntPrivateForeach) foreach, &up);
	assert(up);
	nt_private_foreach(priv, true, (ntPrivateForeach) foreach, &up);
	assert(!up);

	// Test destructor delete
	assert(nt_private_set(priv, "foo", (void *) 0x1234, incDeleted)); // Set
	nt_private_free(priv);  // Delete
	assert(deleted == 4);

	// Cleanup
	assert(global.get("x").isUndefined());
	return 0;
}
