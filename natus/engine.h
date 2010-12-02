/*
 * Copyright 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * This file is part of Natus.
 *
 * Natus is free software: you can redistribute it and/or modify it under the
 * terms of either 1. the GNU General Public License version 2.1, as published
 * by the Free Software Foundation or 2. the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3.0 of the
 * License, or (at your option) any later version.
 *
 * Natus is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with Natus. If not, see
 * http://www.gnu.org/licenses/.
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
