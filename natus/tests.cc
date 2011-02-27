/*
 * Copyright (c) 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include <cassert>
#include <iostream>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus.h>
using namespace natus;

static Value firstarg_function(Value& ths, Value& fnc, vector<Value>& arg) {
	assert(ths.isGlobal() || ths.isUndefined());
	assert(fnc.isFunction());
	assert(arg.size() > 0);
	return arg[0];
}

static Value exception_function(Value& ths, Value& fnc, vector<Value>& arg) {
	return firstarg_function(ths, fnc, arg).toException();
}

class TestClass : public Class {
public:
	Class::Flags getFlags() {
		return Class::FlagAll;
	}

	Value del      (Value& obj, string name) {
		assert(obj.isObject());
		obj.setPrivate("test", (void *) name.length());
		return obj.newBool(true);
	}

	Value del      (Value& obj, long idx) {
		assert(obj.isObject());
		obj.setPrivate("test", (void *) idx);
		return obj.newBool(true);
	}

	Value get      (Value& obj, string name) {
		assert(obj.isObject());
		return obj.newString(name);
	}

	Value get      (Value& obj, long idx) {
		assert(obj.isObject());
		return obj.newNumber(idx);
	}

	Value  set      (Value& obj, string name, Value& value) {
		assert(obj.isObject());
		obj.setPrivate("test", (void *) value.toInt());
		return obj.newBool(true);
	}

	Value  set      (Value& obj, long idx, Value& value) {
		assert(obj.isObject());
		obj.setPrivate("test", (void *) value.toInt());
		return obj.newBool(true);
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
	Engine engine;

	Value x;
	Value y;
	vector<string> strv;
	vector<Value> valv;

	assert(engine.initialize(argv[1]));
	Value global = engine.newGlobal();

	//// Global
	assert(global.isGlobal());
	assert(!global.get("JSON").isGlobal());

	//// Array
	valv = vector<Value>();
	valv.push_back(global.newNumber(123));
	valv.push_back(global.newNumber(456));
	assert(global.set("x", global.newArray(valv)));
	x = global.get("x");
	assert(x.isArray());
	assert(2 == x.get("length").toLong());
	vector<Value> pushargs;
	pushargs.push_back(global.newString("foo"));
	assert(3 == x.call("push", pushargs).toLong());
	y = x.call("pop");
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
	assert(global.set("x", global.newFunction(firstarg_function)));

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
	assert(global.set("x", global.newFunction(exception_function)));

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
	assert(global.set("x", global.newObject(new TestClass())));
	x = global.get("x");
	assert(x.isObject());
	// TODO: enum

	// Delete
	assert(x.del(7));
	assert(7 == (long) x.getPrivate("test"));
	assert(x.del("foo"));
	assert(string("foo").length() == (size_t) x.getPrivate("test"));

	// Get
	y = x.get("foo");
	assert(y.isString());
	assert(y.toString() == "foo");
	y = x.get(7);
	assert(y.isNumber());
	assert(7 == y.toInt());

	// Set
	assert(x.set("foo", x.newNumber(7)));
	assert((void *) 7 == x.getPrivate("test"));
	assert(x.set(12345, x.newNumber(14)));
	assert((void *) 14 == x.getPrivate("test"));

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
	assert(x.setPrivate("test", (void *) 0x1234));
	assert((void *) 0x1234 == x.getPrivate("test"));

	assert(global.del("x"));

	//// String
	assert(global.set("x", global.newString("hello")));
	assert(global.get("x").isString());
	assert(global.get("x").toString() == "hello");
	assert(global.del("x"));

	//// JSON
	x = fromJSON(global, "{\"a\": 17, \"b\": 2.1}");
	assert(!x.isException());
	assert(x.isObject());
	assert(x.get("a").toInt() == 17);
	assert(x.get("b").toDouble() == 2.1);
	assert(toJSON(x) == "{\"a\":17,\"b\":2.1}");

	return 0;
}
