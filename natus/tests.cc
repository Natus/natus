#include <cassert>
#include <iostream>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus.h>
using namespace natus;

static Value firstarg_function(Value& ths, Value& fnc, vector<Value>& arg, void* msc) {
	assert(ths.isGlobal() || ths.isUndefined());
	assert(fnc.isFunction());
	assert(arg.size() > 0);
	return arg[0];
}

static Value exception_function(Value& ths, Value& fnc, vector<Value>& arg, void* msc) {
	return firstarg_function(ths, fnc, arg, msc).toException();
}

class TestClass : public Class {
public:
	TestClass(Class::Flags flags) : Class(flags) {}

	bool  del      (Value& obj, string name) {
		assert(obj.isObject());
		return obj.setPrivate((void *) name.length());
	}

	Value get      (Value& obj, string name) {
		assert(obj.isObject());
		return obj.newString(name);
	}

	Value get      (Value& obj, long idx) {
		assert(obj.isObject());
		return obj.newNumber(idx);
	}

	bool  set      (Value& obj, string name, Value& value) {
		assert(obj.isObject());
		return obj.setPrivate((void *) value.toInt());
	}

	bool  set      (Value& obj, long idx, Value& value) {
		assert(obj.isObject());
		return obj.setPrivate((void *) value.toInt());
	}

	Value enumerate(Value& obj) {
		assert(obj.isObject());
		vector<Value> ret = vector<Value>();
		ret.push_back(obj.newNumber(5));
		ret.push_back(obj.newNumber(10));
		return obj.newArray(ret);
	}

	Value call     (Value& obj, vector<Value> args) {
		assert(obj.isObject());
		assert(args.size() > 0);
		return args[0];
	}

	Value callNew  (Value& obj, vector<Value> args) {
		assert(obj.isObject());
		assert(args.size() > 0);
		return args[0];
	}
};

int main(int argc, char **argv) {
	Value x, y;
	vector<string> strv;
	vector<Value> valv;

	Engine engine;
	assert(engine.initialize(argv[1]));
	Value global = engine.newGlobal(vector<string>(), vector<string>());

	//// Array
	valv = vector<Value>();
	valv.push_back(global.newNumber(123));
	valv.push_back(global.newNumber(456));
	assert(global.set("x", global.newArray(valv)));
	x = global.get("x");
	assert(x.isArray());
	assert(2 == x.length());
	assert(3 == x.push(global.newString("foo")));
	y = x.pop();
	assert(y.isString());
	assert(y.toString() == "foo");
	assert(123 == x.get(0).toLong());
	assert(x.set(0, x.newNumber(789)));
	assert(789 == x.get(0).toLong());
	strv = x.toStringVector();
	assert(strv[0] == "789");
	assert(strv[1] == "456");
	assert(global.del("x"));

	//// Boolean
	assert(global.set("x", global.newBool(true)));
	assert(global.get("x").toBool());
	assert(global.set("x", global.newBool(false)));
	assert(!global.get("x").toBool());
	global.del("x");

	//// Function
	assert(global.set("x", global.newFunction(firstarg_function, NULL, NULL)));

	// Call from C++
	valv = vector<Value>();
	valv.push_back(global.newNumber(123));
	x = global.call("x", valv);
	assert(!x.isException());
	assert(123 == x.toInt());

	// New from C++
	valv = vector<Value>();
	valv.push_back(global.newObject());
	x = global.callNew("x", valv);
	assert(!x.isException());
	assert(x.isObject());

	// Call from JS
	x = global.evaluate("x(123);");
	assert(!x.isException());
	assert(123 == x.toInt());

	// New from JS
	x = global.evaluate("new x({});");
	assert(!x.isException());
	assert(x.isObject());

	assert(global.del("x"));
	assert(global.set("x", global.newFunction(exception_function, NULL, NULL)));

	// Exception Call from C++
	valv = vector<Value>();
	valv.push_back(global.newNumber(123));
	x = global.call("x", valv);
	assert(x.isException());

	// Exception New from C++
	valv = vector<Value>();
	valv.push_back(global.newObject());
	x = global.callNew("x", valv);
	assert(x.isException());

	// Exception Call from JS
	x = global.evaluate("x(123);");
	assert(x.isException());

	// Exception New from JS
	x = global.evaluate("new x({});");
	assert(x.isException());
	assert(global.del("x"));

	//// Null
	global.set("x", global.newNull());
	x = global.get("x");
	assert(x.isNull());
	assert(global.del("x"));

	//// Number
	assert(global.set("x", global.newNumber(3)));
	assert(3 == global.get("x").toInt());
	assert(global.set("x", global.newNumber(3.1)));
	assert(3.1 == global.get("x").toDouble());
	assert(global.del("x"));

	//// Object
	assert(global.set("x", global.newObject(new TestClass(Class::Callable))));
	x = global.get("x");
	assert(x.isObject());
	// TODO: enum

	// Delete
	// TODO: delete from array
	assert(x.del("foo"));
	assert(string("foo").length() == (size_t) x.getPrivate());

	// Get
	y = x.get("foo");
	assert(y.isString());
	assert(y.toString() == "foo");
	y = x.get(7);
	assert(y.isNumber());
	assert(7 == y.toInt());

	// Set
	assert(x.set("foo", x.newNumber(7)));
	assert((void *) 7 == x.getPrivate());
	assert(x.set(12345, x.newNumber(14)));
	assert((void *) 14 == x.getPrivate());

	// Call from C++
	valv = vector<Value>();
	valv.push_back(global.newNumber(123));
	y = global.call("x", valv);
	assert(!y.isException());
	assert(123 == y.toInt());

	// New from C++
	valv = vector<Value>();
	valv.push_back(global.newObject());
	y = global.callNew("x", valv);
	assert(!y.isException());
	assert(y.isObject());

	// Call from JS
	y = global.evaluate("x(123);");
	assert(!y.isException());
	assert(123 == y.toInt());

	// New from JS
	y = global.evaluate("new x({});");
	assert(!y.isException());
	assert(y.isObject());

	// Private
	assert(x.setPrivate((void *) 0x1234));
	assert((void *) 0x1234 == x.getPrivate());

	assert(global.del("x"));

	//// String
	assert(global.set("x", global.newString("hello")));
	assert(global.get("x").isString());
	assert(global.get("x").toString() == "hello");
	assert(global.del("x"));

	return 0;
}
