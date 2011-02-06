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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "engine.h"
namespace natus {

EngineValue::EngineValue(EngineValue* glb, bool exception) {
	this->glb = glb;
	refCnt = 0;
	exc = exception;
	if (this != glb) glb->incRef();
}

void EngineValue::incRef() {
	refCnt++;
}

void EngineValue::decRef() {
	refCnt = refCnt > 0 ? refCnt-1 : 0;
	if (refCnt == 0)
		delete this;
}

Value EngineValue::toException(Value& value) {
	borrowInternal<EngineValue>(value)->exc = true;
	return value;
}

bool EngineValue::isException() {
	return exc;
}

Value EngineValue::getGlobal() {
	return Value(glb);
}

EngineValue::~EngineValue() {
	if (this != glb) glb->decRef();
}

ClassFuncPrivate::ClassFuncPrivate(EngineValue* glbl, Class* clss) {
	this->clss = clss;
	this->func = NULL;
	this->glbl = glbl;
}

ClassFuncPrivate::ClassFuncPrivate(EngineValue* glbl, NativeFunction func) {
	this->clss = NULL;
	this->func = func;
	this->glbl = glbl;
}

ClassFuncPrivate::~ClassFuncPrivate() {
	if (clss)
		delete clss;
	for (map<string, pair<void*, FreeFunction> >::iterator it=priv.begin() ; it != priv.end() ; it++)
		if (it->second.first && it->second.second)
			it->second.second(it->second.first);
}

}
