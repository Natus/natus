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

#ifndef NATUS_ENGINE_H_
#define NATUS_ENGINE_H_
#include <map>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
namespace natus {

#define NATUS_ENGINE_VERSION 1
#define NATUS_ENGINE_ natus_engine__
#define NATUS_ENGINE(name, symb, init, newg, free) \
	EngineSpec NATUS_ENGINE_ = { NATUS_ENGINE_VERSION, name, symb, init, newg, free }

class EngineValue;

typedef pair<void*, FreeFunction> PrivateItem;
typedef map<string, PrivateItem>  PrivateMap;

struct ClassFuncPrivate {
	Class*         clss;
	NativeFunction func;
	PrivateMap     priv;
	EngineValue*   glbl;

	virtual ~ClassFuncPrivate() {
		if (clss)
			delete clss;
		for (map<string, pair<void*, FreeFunction> >::iterator it=priv.begin() ; it != priv.end() ; it++)
			if (it->second.second)
				it->second.second(it->second.first);
	}
};

class EngineValue {
public:
	// Methods that mirror Value's methods
	virtual Value            newBool(bool b)=0;
	virtual Value            newNumber(double n)=0;
	virtual Value            newString(string string)=0;
	virtual Value            newArray(vector<Value> array)=0;
	virtual Value            newFunction(NativeFunction func)=0;
	virtual Value            newObject(Class* cls)=0;
	virtual Value            newNull()=0;
	virtual Value            newUndefined()=0;
	virtual Value            getGlobal();
	virtual void             getContext(void **context, void **value)=0;

	virtual bool             isGlobal()=0;
	virtual bool             isException();
	virtual bool             isArray()=0;
	virtual bool             isBool()=0;
	virtual bool             isFunction()=0;
	virtual bool             isNull()=0;
	virtual bool             isNumber()=0;
	virtual bool             isObject()=0;
	virtual bool             isString()=0;
	virtual bool             isUndefined()=0;

	virtual bool             toBool()=0;
	virtual double           toDouble()=0;
	virtual string           toString()=0;

	virtual bool             del(string name)=0;
	virtual bool             del(long idx)=0;
	virtual Value            get(string name)=0;
	virtual Value            get(long idx)=0;
	virtual bool             set(string name, Value value, Value::PropAttrs attrs=Value::None)=0;
	virtual bool             set(long idx, Value value)=0;
	virtual std::set<string> enumerate()=0;

	virtual Value            evaluate(string jscript, string filename="", unsigned int lineno=0, bool shift=false)=0;
	virtual Value            call(Value func, vector<Value> args)=0;
	virtual Value            callNew(vector<Value> args)=0;

	// Non-Value methods that need to be implemented by Engines
	virtual bool             supportsPrivate()=0;

	// Non-Value methods implemented here
	EngineValue(EngineValue* glb, bool exception=false);
	void                     incRef();
	void                     decRef();
	Value                    toException(Value& value);
	virtual PrivateMap*      getPrivateMap()=0;

protected:
	unsigned long  refCnt;
	bool           exc;
	EngineValue*   glb;

	template <class T> static T* borrowInternal(Value& value) {
		return static_cast<T*>(value.internal);
	}
};

struct EngineSpec {
	unsigned int  vers;
	const char   *name;
	const char   *symb;
	void*       (*init)();
	EngineValue*(*newg)(void *);
	void        (*free)(void *);
};

}  // namespace natus
#endif /* NATUS_ENGINE_H_ */
