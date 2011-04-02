#include "test.hpp"

class TestClass : public Class {
public:
	Class::Flags getFlags() {
		return Class::FlagFunction;
	}

	virtual Value del(Value& obj, Value& name) {
		assert(obj.isObject());
		if (name.toStringUTF8() == "exc")
			return obj.newString("error").toException();
		if (name.toStringUTF8() == "oom")
			return NULL;
		if (name.isNumber())
			obj.setPrivate("test", (void *) name.toLong());
		else
			obj.setPrivate("test", (void *) name.toStringUTF8().length());
		return obj.newBool(true);
	}

	virtual Value get(Value& obj, Value& name) {
		assert(obj.isObject());
		if (name.toStringUTF8() == "exc")
			return obj.newString("error").toException();
		if (name.toStringUTF8() == "oom")
			return NULL;
		return name;
	}

	virtual Value set(Value& obj, Value& name, Value& value) {
		assert(obj.isObject());
		if (name.toStringUTF8() == "exc")
			return obj.newString("error").toException();
		if (name.toStringUTF8() == "oom")
			return NULL;
		obj.setPrivate("test", (void *) value.toLong());
		return obj.newBool(true);
	}

	virtual Value enumerate(Value& obj) {
		assert(obj.isObject());
		return obj.getPrivate<Value>("enumerate");
	}

	virtual Value call(Value& obj, Value& ths,  Value& args) {
		assert(obj.isObject());
		assert(args.isArray());
		assert(args.get("length").toLong() > 0);
		return args.get(0);
	}
};

int doTest(Engine& engine, Value& global) {
	assert(!global.set("x", global.newObject(new TestClass())).isException());
	Value x = global.get("x");
	assert(x.isObject());

	// Delete
	assert(!x.del(7).isException());
	assert(7 == x.getPrivate<long>("test"));
	assert(!x.del("foo").isException());
	assert(strlen("foo") == x.getPrivate<size_t>("test"));

	// Get
	Value y = x.get("foo");
	assert(y.isString());
	assert(y.toStringUTF8() == "foo");
	y = x.get(7);
	assert(y.isNumber());
	assert(7 == y.toLong());

	// Set
	assert(!x.set("foo", x.newNumber(7)).isException());
	assert(7 == x.getPrivate<long>("test"));
	assert(!x.set(12345, x.newNumber(14)).isException());
	assert(14 == x.getPrivate<long>("test"));

	// Call from C++
	y = global.call("x", global.newArrayBuilder(global.newNumber(123)));
	assert(!y.isException());
	assert(123 == y.toLong());

	// New from C++
	y = global.callNew("x", global.newArrayBuilder(global.newObject()));
	assert(!y.isException());
	assert(y.isObject());

	// Call from JS
	y = global.evaluate("x(123);");
	assert(!y.isException());
	assert(123 == y.toLong());

	// New from JS
	y = global.evaluate("new x({});");
	assert(!y.isException());
	assert(y.isObject());

	// Enumerate
	x.setPrivate("enumerate", x.newArrayBuilder(x.newNumber(5)).newArrayBuilder(x.newNumber(10)));
	y = x.enumerate();
	assert(y.isArray());
	assert(y.get("length").toLong() == 2);
	assert(y.get(0).toLong() == 5);
	assert(y.get(1).toLong() == 10);

	// Now we check for fault states
	/*assert(!x.del("exc"));
	//assert(!x.del("oom"));
	assert(x.get("exc").isException());
	//assert(x.get("oom").isOOM());
	assert(!x.set("exc", x.newNumber(5)));
	//assert(!x.set("oom", x.newNumber(5)));
	x.setPrivate("enumerate", x.newString("error").toException());
	assert(x.enumerate().isException());
	x.setPrivate("enumerate", NULL);
	assert(x.enumerate().isOOM());*/

	// Cleanup
	assert(!global.del("x").isException());
	assert(global.get("x").isUndefined());
	return 0;
}
