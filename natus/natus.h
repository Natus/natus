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

typedef Value (*RequireFunction)(Value& module, string& name, string& reldir, vector<string>& path, void* misc);

class Class {
public:
	/* Please note that currently only Object and Callable
	 * actually do anything.  We may change this later.
	 */
	typedef enum {
		FlagNone           = 0,
		FlagDeleteItem     = 1 << 0,
		FlagDeleteProperty = 1 << 1,
		FlagGetItem        = 1 << 2,
		FlagGetProperty    = 1 << 3,
		FlagSetItem        = 1 << 4,
		FlagSetProperty    = 1 << 5,
		FlagEnumerate      = 1 << 6,
		FlagCall           = 1 << 7,
		FlagNew            = 1 << 8,
		FlagDelete         = FlagDeleteItem | FlagDeleteProperty,
		FlagGet            = FlagGetItem    | FlagGetProperty,
		FlagSet            = FlagSetItem    | FlagSetProperty,
		FlagArray          = FlagDeleteItem     | FlagGetItem     | FlagSetItem     | FlagEnumerate,
		FlagObject         = FlagDeleteProperty | FlagGetProperty | FlagSetProperty | FlagEnumerate,
		FlagFunction       = FlagCall | FlagNew,
		FlagAll            = FlagArray | FlagObject | FlagFunction,
	} Flags;

	virtual Class::Flags getFlags ();
	/*
	 * * Undefined Exception: don't intercept
	 * * Other Exception: throw exception
	 * * Otherwise: success
	 */
	virtual Value        del      (Value& obj, string name);
	virtual Value        del      (Value& obj, long idx);

	/*
	 * * Undefined Exception: don't intercept
	 * * Other Exception: throw exception
	 * * Otherwise: returns value
	 */
	virtual Value        get      (Value& obj, string name);
	virtual Value        get      (Value& obj, long idx);

	/*
	 * * Undefined Exception: don't intercept
	 * * Other Exception: throw exception
	 * * Otherwise: success
	 */
	virtual Value        set      (Value& obj, string name, Value& value);
	virtual Value        set      (Value& obj, long idx, Value& value);


	virtual Value        enumerate(Value& obj);
	virtual Value        call     (Value& obj, vector<Value> args);
	virtual Value        callNew  (Value& obj, vector<Value> args);
	virtual ~Class();
};

class Engine {
public:
	Engine();
	~Engine();
	bool   initialize(const char* name_or_path);
	bool   initialize();
	string getName();

	Value  newGlobal(string config="{}");

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

	Value            newBool(bool b) const;
	Value            newNumber(double n) const;
	Value            newString(string string) const;
	Value            newArray(vector<Value> array=vector<Value>()) const;
	Value            newFunction(NativeFunction func) const;
	Value            newObject(Class* cls=NULL) const;
	Value            newNull() const;
	Value            newUndefined() const;
	Value            newException(string type, string message) const;
	Value            newException(string type, string message, long code) const;
	Value            newException(int errorno) const;
	Value            getGlobal() const;
	void             getContext(void **context, void **value) const;

	bool             isGlobal() const;
	bool             isException() const;
	bool             isOOM() const;
	bool             isArray() const;
	bool             isBool() const;
	bool             isFunction() const;
	bool             isNull() const;
	bool             isNumber() const;
	bool             isObject() const;
	bool             isString() const;
	bool             isUndefined() const;

	bool             toBool() const;
	double           toDouble() const;
	Value            toException();
	int              toInt() const;
	long             toLong() const;
	string           toString() const;
	vector<string>   toStringVector() const;

	bool             del(string name);
	bool             del(long idx);
	Value            get(string name) const;
	Value            get(long idx) const;
	bool             has(string name) const;
	bool             has(long idx) const;
	bool             set(string name, Value value, Value::PropAttrs attrs=None, bool makeParents=false);
	bool             set(string name, int value, Value::PropAttrs attrs=None, bool makeParents=false);
	bool             set(string name, double value, Value::PropAttrs attrs=None, bool makeParents=false);
	bool             set(string name, string value, Value::PropAttrs attrs=None, bool makeParents=false);
	bool             set(string name, NativeFunction value, Value::PropAttrs attrs=None, bool makeParents=false);
	bool             set(long idx, Value value);
	bool             set(long idx, int value);
	bool             set(long idx, double value);
	bool             set(long idx, string value);
	bool             set(long idx, NativeFunction value);
	std::set<string> enumerate() const;

	long             length() const;
	long             push(Value value);
	Value            pop();

	bool             setPrivate(string key, void *priv, FreeFunction free);
	bool             setPrivate(string key, void *priv);
	void*            getPrivate(string key) const;

	Value            evaluate(string jscript, string filename="", unsigned int lineno=0, bool shift=false);
	Value            call(Value func);
	Value            call(string func);
	Value            call(Value func, vector<Value> args);
	Value            call(string func, vector<Value> args);
	Value            callNew();
	Value            callNew(string func);
	Value            callNew(vector<Value> args);
	Value            callNew(string func, vector<Value> args);

	Value            fromJSON(string json);
	string           toJSON();

	Value            getConfig() const;
	Value            checkArguments(vector<Value>& arg, const char* fmt) const;
	Value            require(string name, string reldir, vector<string> path);
	void             addRequireHook(bool post, RequireFunction func, void* misc=NULL, FreeFunction free=NULL);

protected:
	EngineValue *internal;
};

}  // namespace natus
#endif /* NATUS_H_ */

