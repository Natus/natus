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
#include <stdarg.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
namespace natus {

#define NATUS_ENGINE_VERSION 1
#define NATUS_ENGINE_ natus_engine__
#define NATUS_ENGINE(name, symb, init, newg, free) \
	EngineSpec NATUS_ENGINE_ = { NATUS_ENGINE_VERSION, name, symb, init, newg, free }

class EngineValue : virtual public BaseValue<Value> {
public:
	EngineValue(EngineValue* glb, bool exception=false);
	void   incRef();
	void   decRef();
	Value  toException(Value& value);
	bool   isException();
	Value  getGlobal();
	virtual bool supportsPrivate()=0;

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

struct ClassFuncPrivate {
	Class*         clss;
	NativeFunction func;
	void*          priv;
	FreeFunction   free;
	EngineValue*   glbl;

	~ClassFuncPrivate() {
		if (clss)
			delete clss;
		if (free)
			free(priv);
	}
};

}  // namespace natus
#endif /* NATUS_ENGINE_H_ */
