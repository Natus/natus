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

#include <cerrno>

#include <cstdlib>
#include <cstring>
#include <libgen.h>

#include <iostream>
#include <sstream>


namespace natus {

static void freeValue(void* value) {
	delete static_cast<Value*>(value);
}

Value::Value() {
	internal = NULL;
}

Value::Value(EngineValue* value) {
	internal = value;
	if (internal)
		internal->incRef();
}

Value::Value(const Value& value) {
	internal = value.internal;
	if (internal)
		internal->incRef();
}

Value::~Value() {
	if (internal) internal->decRef();
}

Value& Value::operator=(const Value& value) {
	if (internal) internal->decRef();
	internal = value.internal;
	if (internal) internal->incRef();
	return *this;
}

Value Value::operator[](long index) {
	return get(index);
}

Value Value::operator[](string name) {
	return get(name);
}

Value Value::newBool(bool b) const {
	return internal->newBool(b);
}

Value Value::newNumber(double n) const {
	return internal->newNumber(n);
}

Value Value::newString(string string) const {
	return internal->newString(string);
}

Value Value::newArray(vector<Value> array) const {
	return internal->newArray(array);
}

Value Value::newFunction(NativeFunction func) const {
	return internal->newFunction(func);
}

Value Value::newObject(Class* cls) const {
	return internal->newObject(cls);
}

Value Value::newNull() const {
	return internal->newNull();
}

Value Value::newUndefined() const {
	return internal->newUndefined();
}

Value Value::getGlobal() const {
	return internal->getGlobal();
}

void Value::getContext(void **context, void **value) const {
	internal->getContext(context, value);
}

bool Value::isGlobal() const {
	return internal->isGlobal();
}

bool Value::isException() const {
	return internal == NULL || internal->isException();
}

bool Value::isOOM() const {
	return internal == NULL;
}

bool Value::isArray() const {
	return internal->isArray();
}

bool Value::isBool() const {
	return internal->isBool();
}

bool Value::isFunction() const {
	return internal->isFunction();
}

bool Value::isNull() const {
	return internal->isNull();
}

bool Value::isNumber() const {
	return internal->isNumber();
}

bool Value::isObject() const {
	return internal->isObject();
}

bool Value::isString() const {
	return internal->isString();
}

bool Value::isUndefined() const {
	return internal->isUndefined();
}

bool Value::toBool() const {
	if (isUndefined()) return false;
	return internal->toBool();
}

double Value::toDouble() const {
	if (isUndefined()) return 0;
	return internal->toDouble();
}

Value Value::toException() {
	return internal->toException(*this);
}

int Value::toInt() const {
	if (isUndefined()) return 0;
	return (int) toDouble();
}

long Value::toLong() const {
	if (isUndefined()) return 0;
	return (long) toDouble();
}

string Value::toString() const {
	return internal->toString();
}

vector<string> Value::toStringVector() const {
	vector<string> strv = vector<string>();
	long len = get("length").toLong();
	for (long i=0 ; i < len ; i++)
		strv.push_back(get(i).toString());
	return strv;
}

bool Value::del(string name) {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	return internal->del(name);
}

bool Value::del(long idx) {
	if (!isObject() && !isArray())
		return false;
	return internal->del(idx);
}

Value Value::get(string name) const {
	if (!isArray() && !isFunction() && !isObject())
		return newUndefined();

	if (name.find_first_of('.') == string::npos)
		return internal->get(name);

	return get(name.substr(0, name.find_first_of('.'))).get(name.substr(name.find_first_of('.')+1));
}

Value Value::get(long idx) const {
	if (!isObject() && !isArray())
		return newUndefined();
	return internal->get(idx);
}

bool Value::has(string name) const {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	Value v = get(name);
	return !v.isUndefined();
}

bool Value::has(long idx) const {
	if (!isObject() && !isArray())
		return false;
	Value v = get(idx);
	return !v.isUndefined();
}

bool Value::set(string name, Value value, Value::PropAttrs attrs, bool makeParents) {
	if (!isArray() && !isFunction() && !isObject())
		return false;

	if (name.find_first_of('.') == string::npos)
		return internal->set(name, value, attrs);

	Value next = get(name.substr(0, name.find_first_of('.')));
	if (makeParents && next.isUndefined()) {
		next = newObject();
		if (!set(name.substr(0, name.find_first_of('.')), next))
			return false;
	}

	return next.set(name.substr(name.find_first_of('.')+1), value, attrs, makeParents);
}

bool Value::set(string name, int value, Value::PropAttrs attrs, bool makeParents) {
	return set(name, newNumber(value), attrs, makeParents);
}

bool Value::set(string name, double value, Value::PropAttrs attrs, bool makeParents) {
	return set(name, newNumber(value), attrs, makeParents);
}

bool Value::set(string name, string value, Value::PropAttrs attrs, bool makeParents) {
	return set(name, newString(value), attrs, makeParents);
}

bool Value::set(string name, NativeFunction value, Value::PropAttrs attrs, bool makeParents) {
	return set(name, newFunction(value), attrs, makeParents);
}

bool Value::set(long idx, Value value) {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	return internal->set(idx, value);
}

bool Value::set(long idx, int value) {
	return set(idx, newNumber(value));
}

bool Value::set(long idx, double value) {
	return set(idx, newNumber(value));
}

bool Value::set(long idx, string value) {
	return set(idx, newString(value));
}

bool Value::set(long idx, NativeFunction value) {
	return set(idx, newFunction(value));
}

set<string> Value::enumerate() const {
	if (!isArray() && !isFunction() && !isObject())
		return std::set<string>();
	return internal->enumerate();
}

bool Value::setPrivate(string key, void *priv, FreeFunction free) {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return false;
	if (!(pm = internal->getPrivateMap())) return false;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end()) {
		if (it->second.second)
			it->second.second(it->second.first);
		pm->erase(it);
	}
	PrivateItem pi(priv, free);
	pm->insert(pair<string, PrivateItem>(key, pi));
	return true;
}

bool Value::setPrivate(string key, void *priv) {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return false;
	if (!(pm = internal->getPrivateMap())) return false;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end())
		return setPrivate(key, priv, it->second.second);
	return setPrivate(key, priv, NULL);
}

bool Value::setPrivate(string key, Value value) {
	return setPrivate(key, new Value(value), freeValue);
}

void* Value::getPrivate(string key) const {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return NULL;
	if (!(pm = internal->getPrivateMap())) return NULL;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end())
		return it->second.first;
	return NULL;
}

Value Value::getPrivateValue(string key) const {
	Value* v = static_cast<Value*>(getPrivate(key));
	if (!v) return newUndefined();
	return *v;
}

Value Value::evaluate(string jscript, string filename, unsigned int lineno, bool shift) {
	if (shift && !isObject() && !isFunction()) return newUndefined();

	if (jscript.substr(0, 2) == "#!" && jscript.find_first_of('\n') != string::npos)
		jscript = jscript.substr(jscript.find_first_of('\n')+1);

	// Add the file's directory to the require stack
	char *tmp = NULL;
	Value reqStack = this->getGlobal().get("require").getPrivateValue("natus.reqStack");
	if (reqStack.isArray()) {
		tmp = strdup(filename.c_str());
		if (tmp) {
			vector<Value> pushargs;
			pushargs.push_back(reqStack.newString(dirname(tmp)));
			std::free(tmp);
			reqStack.call("push", pushargs);
		}
	}

	Value ret = internal->evaluate(jscript, filename, lineno, shift);

	// Remove the directory from the stack
	if (reqStack.isArray() && tmp) reqStack.call("pop");
	return ret;
}

Value Value::call(Value func) {
	vector<Value> args = vector<Value>();
	return call(func, args);
}

Value Value::call(string func) {
	vector<Value> args = vector<Value>();
	return call(func, args);
}

Value Value::call(Value func, vector<Value> args) {
	if (!func.isObject() && !func.isFunction())
		return newUndefined();
	return internal->call(func, args);
}

Value Value::call(string func, vector<Value> args) {
	Value v = get(func);
	return call(v, args);
}

Value Value::callNew() {
	vector<Value> args = vector<Value>();
	return callNew(args);
}

Value Value::callNew(string func) {
	vector<Value> args = vector<Value>();
	return callNew(func, args);
}

Value Value::callNew(vector<Value> args) {
	if (!isObject() && !isFunction())
		return newUndefined();
	return internal->callNew(args);
}

Value Value::callNew(string func, vector<Value> args) {
	return get(func).callNew(args);
}

Value   Value::fromJSON(string json) {
	vector<Value> args;
	args.push_back(newString(json));
	Value obj = getGlobal().get("JSON");
	return obj.call("parse", args);
}

string  Value::toJSON() {
	vector<Value> args;
	args.push_back(*this);
	Value obj = getGlobal().get("JSON");
	return obj.call("stringify", args).toString();
}

}
