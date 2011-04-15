#include "test.hpp"

class TestClass : public Class {
public:
	Class::Flags getFlags() {
		return Class::FlagFunction;
	}

	virtual Value del(Value& obj, Value& name) {
		assert(obj.borrowCValue());
		assert(name.borrowCValue());
		assert(obj.isObject());
		assert(name.getType() == obj.getPrivate<size_t>("type"));
		return obj.getPrivate<Value>("retval");
	}

	virtual Value get(Value& obj, Value& name) {
		assert(obj.borrowCValue());
		assert(name.borrowCValue());
		assert(obj.isObject());
		assert(name.getType() == obj.getPrivate<size_t>("type"));
		return obj.getPrivate<Value>("retval");
	}

	virtual Value set(Value& obj, Value& name, Value& value) {
		assert(obj.borrowCValue());
		assert(name.borrowCValue());
		assert(value.borrowCValue());
		assert(obj.isObject());
		assert(name.getType() == obj.getPrivate<size_t>("type"));
		return obj.getPrivate<Value>("retval");
	}

	virtual Value enumerate(Value& obj) {
		assert(obj.borrowCValue());
		assert(obj.isObject());
		return obj.getPrivate<Value>("retval");
	}

	virtual Value call(Value& obj, Value& ths, Value& args) {
		assert(obj.borrowCValue());
		assert(ths.borrowCValue());
		assert(args.borrowCValue());
		assert(obj.isObject());
		assert(args.isArray());
		if (strcmp(obj.getEngine().getName(), "v8")) // Work around a bug in v8
			assert(ths.isGlobal() || ths.isUndefined());
		if (args.get("length").to<int>() == 0)
			return obj.getPrivate<Value>("retval");
		return args[0];
	}
};

int doTest(Engine& engine, Value& global) {
	assert(!global.set("x", global.newObject(new TestClass())).isException());
	Value x = global.get("x");
	assert(x.isObject());

	//// Check for bypass
	x.setPrivate("retval", NULL); // Ensure NULL
	// Number, native
	assert(x.setPrivate("type", (void*) Value::TypeNumber));
	assert(!x.set(0, 17).isException());
	assert(!x.get(0).isException());
	assert( x.get(0).to<int>() == 17);
	assert(!x.del(0).isException());
	assert( x.get(0).isUndefined());
	// Number, eval
	assert(!global.evaluate("x[0] = 17;").isException());
	assert(!global.evaluate("x[0];").isException());
	assert( global.evaluate("x[0];").to<int>() == 17);
	assert(!global.evaluate("delete x[0];").isException());
	assert( global.evaluate("x[0];").isUndefined());
	// String, native
	assert(x.setPrivate("type", (void*) Value::TypeString));
	assert(!x.set("foo", 17).isException());
	assert(!x.get("foo").isException());
	assert( x.get("foo").to<int>() == 17);
	assert(!x.del("foo").isException());
	assert( x.get("foo").isUndefined());
	// String, eval
	assert(!global.evaluate("x['foo'] = 17;").isException());
	assert(!global.evaluate("x['foo'];").isException());
	assert( global.evaluate("x['foo'];").to<int>() == 17);
	assert(!global.evaluate("delete x['foo'];").isException());
	assert( global.evaluate("x['foo'];").isUndefined());

	//// Check for exception
	assert(x.setPrivate("retval", global.newString("error").toException()));
	// Number, native
	assert(x.setPrivate("type", (void*) Value::TypeNumber));
	assert(x.del(0).isException());
	assert(x.get(0).isException());
	assert(x.set(0, 0).isException());
	// Number, eval
	assert(global.evaluate("delete x[0];").isException());
	assert(global.evaluate("x[0];").isException());
	assert(global.evaluate("x[0] = 0;").isException());
	// String, native
	assert(x.setPrivate("type", (void*) Value::TypeString));
	assert(x.del("foo").isException());
	assert(x.get("foo").isException());
	assert(x.set("foo", 0).isException());
	// String, eval
	assert(global.evaluate("delete x['foo'];").isException());
	assert(global.evaluate("x['foo'];").isException());
	assert(global.evaluate("x['foo'] = 0;").isException());

	//// Check for intercept
	assert(x.setPrivate("retval", global.newBoolean(true)));
	// Number, native
	assert(x.setPrivate("type", (void*) Value::TypeNumber));
	assert(!x.del(0).isException());
	assert(x.get(0).isBool());
	assert(x.get(0).to<bool>());
	assert(!x.set(0, 0).isException());
	// Number, eval
	assert(!global.evaluate("delete x[0];").isException());
	assert(global.evaluate("x[0];").isBool());
	assert(global.evaluate("x[0];").to<bool>());
	assert(!global.evaluate("x[0] = 0;").isException());
	// String, native
	assert(x.setPrivate("type", (void*) Value::TypeString));
	assert(!x.del("foo").isException());
	assert(x.get("foo").isBool());
	assert(x.get("foo").to<bool>());
	assert(!x.set("foo", 0).isException());
	// String, eval
	assert(!global.evaluate("delete x['foo'];").isException());
	assert(global.evaluate("x['foo'];").isBool());
	assert(global.evaluate("x['foo'];").to<bool>());
	assert(!global.evaluate("x['foo'] = 0;").isException());

	//// Check for successful calls
	// Call from C++
	Value y = global.call("x", global.newArrayBuilder(123));
	assert(!y.isException());
	assert(y.isNumber());
	assert(y.to<int>() == 123);
	// New from C++
	y = global.callNew("x", global.newArrayBuilder(global.newObject()));
	assert(!y.isException());
	assert(y.isObject());
	// Call from JS
	y = global.evaluate("x(123);");
	assert(!y.isException());
	assert(y.isNumber());
	assert(y.to<int>() == 123);
	// New from JS
	y = global.evaluate("new x({});");
	assert(!y.isException());
	assert(y.isObject());

	//// Check for exception calls
	assert(x.setPrivate("retval", global.newString("error").toException()));
	// Call from C++
	y = global.call("x");
	assert(y.isString());
	assert(y.isException());
	// New from C++
	y = global.callNew("x");
	assert(y.isString());
	assert(y.isException());
	// Call from JS
	y = global.evaluate("x();");
	assert(y.isString());
	assert(y.isException());
	// New from JS
	y = global.evaluate("new x();");
	assert(y.isString());
	assert(y.isException());

	//// Check for NULL calls
	assert(x.setPrivate("retval", NULL)); // Ensure NULL
	// Call from C++
	y = global.call("x");
	assert(y.isUndefined());
	assert(y.isException());
	// New from C++
	y = global.callNew("x");
	assert(y.isUndefined());
	assert(y.isException());
	// Call from JS
	y = global.evaluate("x();");
	assert(y.isUndefined());
	assert(y.isException());
	// New from JS
	y = global.evaluate("new x();");
	assert(y.isUndefined());
	assert(y.isException());

	// Enumerate
	assert(x.setPrivate("retval", x.newArrayBuilder(x.newNumber(5)).newArrayBuilder(x.newNumber(10))));
	y = x.enumerate();
	assert(y.isArray());
	assert(y.get("length").to<int>() == 2);
	assert(y.get(0).to<int>() == 5);
	assert(y.get(1).to<int>() == 10);

	// Cleanup
	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}
