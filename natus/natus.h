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

#ifndef NATUS_H_
#define NATUS_H_

#ifndef I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#error Natus is not stable, go look elsewhere...
#endif

#include <set>
#include <string>
#include <vector>

namespace natus {
using namespace std;
class Value;
class EngineValue;

/* Type: NativeFunction
 * Function type for calls made back into native space from javascript.
 *
 * Parameters:
 *     ths - The 'this' variable for the current execution.
 *           ths has a value of 'Undefined' when being run as a constructor.
 *     fnc - The object representing the function itself.
 *     arg - The argument vector.
 *     msc - Misc. data, usually specified at function creation time.
 *
 * Returns:
 *     The return value, which may be an exception.
 */
typedef Value (*NativeFunction)(Value& ths, Value& fnc, vector<Value>& arg);

/* Type: FreeFunction
 * Function type for calls made back to free a memory value allocated outside natus.
 *
 * Parameters:
 *     mem - The memory to free.
 */
typedef void (*FreeFunction)(void *mem);

class Class {
public:
	/* Please note that currently only Object and Callable
	 * actually do anything.  We may change this later.
	 */
	typedef enum {
		None      = 0,
		Delete    = 1 << 0,
		Get       = 1 << 1,
		Set       = 1 << 2,
		Enumerate = 1 << 3,
		Call      = 1 << 4,
		New       = 1 << 5,
		Object    = Delete | Get | Set | Enumerate,
		Function  = Call | New,
		Callable  = Function | Object
	} Flags;

	Class(Class::Flags flags=Callable);
	Class::Flags getFlags();

	virtual bool  del      (Value& obj, string name);
	virtual bool  del      (Value& obj, long idx);
	virtual Value get      (Value& obj, string name);
	virtual Value get      (Value& obj, long idx);
	virtual bool  set      (Value& obj, string name, Value& value);
	virtual bool  set      (Value& obj, long idx, Value& value);
	virtual Value enumerate(Value& obj);
	virtual Value call     (Value& obj, vector<Value> args);
	virtual Value callNew  (Value& obj, vector<Value> args);
	virtual ~Class();

protected:
	Class::Flags flags;
};

class Engine {
public:
	Engine();
	~Engine();
	bool   initialize(const char* name_or_path);
	bool   initialize();
	string getName();

	Value  newGlobal(vector<string> path, vector<string> whitelist);
	Value  newGlobal(vector<string> path);
	Value  newGlobal();

private:
	void *internal;
};

class Value {
	friend class EngineValue;

public:
	typedef enum {
		None       = 0,
		ReadOnly   = 1 << 0,
		DontEnum   = 1 << 1,
		DontDelete = 1 << 2,
		Constant   = ReadOnly | DontDelete,
		Protected  = Constant | DontEnum
	} PropAttrs;

	Value();
	Value(EngineValue *value);
	Value(const Value& value);
	~Value();
	Value&           operator=(const Value& value);
	Value            operator[](long index);
	Value            operator[](string index);

	Value            newBool(bool b);
	Value            newNumber(double n);
	Value            newString(string string);
	Value            newArray(vector<Value> array=vector<Value>());
	Value            newFunction(NativeFunction func);
	Value            newObject(Class* cls=NULL);
	Value            newNull();
	Value            newUndefined();
	Value            getGlobal();
	void             getContext(void **context, void **value);

	bool             isGlobal();
	bool             isException();
	bool             isArray();
	bool             isBool();
	bool             isFunction();
	bool             isNull();
	bool             isNumber();
	bool             isObject();
	bool             isString();
	bool             isUndefined();

	bool             toBool();
	double           toDouble();
	Value            toException();
	int              toInt();
	long             toLong();
	string           toString();
	vector<string>   toStringVector();

	bool             del(string name);
	bool             del(long idx);
	Value            get(string name);
	Value            get(long idx);
	bool             has(string name);
	bool             has(long idx);
	bool             set(string name, Value value, Value::PropAttrs attrs=None);
	bool             set(long idx, Value value);
	std::set<string> enumerate();

	long             length();
	long             push(Value value);
	Value            pop();

	bool             setPrivate(string key, void *priv, FreeFunction free);
	bool             setPrivate(string key, void *priv);
	void*            getPrivate(string key);

	Value            evaluate(string jscript, string filename="", unsigned int lineno=0, bool shift=false);
	Value            call(Value func);
	Value            call(string func);
	Value            call(Value func, vector<Value> args);
	Value            call(string func, vector<Value> args);
	Value            callNew();
	Value            callNew(string func);
	Value            callNew(vector<Value> args);
	Value            callNew(string func, vector<Value> args);

	Value            require(string name, string reldir, vector<string> path);
	Value            fromJSON(string json);
	string           toJSON();

protected:
	EngineValue *internal;
};

}  // namespace natus
#endif /* NATUS_H_ */

