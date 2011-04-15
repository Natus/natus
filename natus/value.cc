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

#include <cstdlib> // For free()
#include <cstdio>

#include "engine.hpp"
#include "value.hpp"
#include "value.h"
using namespace natus;

static inline Value** tx(ntValue **args) {
	if (!args) return NULL;

	long len;
	for (len=0 ; args && args[len] ; len++);

	Value **a = new Value*[len+1];
	for (len=0 ; args && args[len] ; len++)
		a[len] = new Value(args[len]);
	a[len] = NULL;

	return a;
}

static inline void txFree(Value **args) {
	for (int i=0 ; args && args[i] ; i++)
		delete args[i];
	delete[] args;
}

static ntValue *txFunction(ntValue *obj, ntValue *ths, ntValue *args) {
	Value o = nt_value_incref(obj);
	Value t = nt_value_incref(ths);
	Value a = nt_value_incref(args);

	NativeFunction f = o.getPrivate<NativeFunction>("natus::Function++");
	if (!f) return NULL;

	Value rslt = f(o, t, a);

	return nt_value_incref(rslt.borrowCValue());
}

struct txClass {
	ntClass hdr;
	Class  *cls;

	static ntValue *del(ntClass *cls, ntValue *obj, const ntValue *prop) {
		Value o(obj, false);
		Value n((ntValue*) prop, false);
		return nt_value_incref(((txClass *) cls)->cls->del(o, n).borrowCValue());
	}

	static ntValue *get(ntClass *cls, ntValue *obj, const ntValue *prop) {
		Value o(obj, false);
		Value n((ntValue*) prop, false);
		return nt_value_incref(((txClass *) cls)->cls->get(o, n).borrowCValue());
	}

	static ntValue *set(ntClass *cls, ntValue *obj, const ntValue *prop, const ntValue *value) {
		Value o(obj, false);
		Value n((ntValue*) prop, false);
		Value v((ntValue*) value, false);
		return nt_value_incref(((txClass *) cls)->cls->set(o, n, v).borrowCValue());
	}

	static ntValue *enumerate   (ntClass *cls, ntValue *obj) {
		Value o(obj, false);
		return nt_value_incref(((txClass *) cls)->cls->enumerate(o).borrowCValue());
	}

	static ntValue *call        (ntClass *cls, ntValue *obj, ntValue *ths, ntValue* args) {
		Value o(obj, false);
		Value t(ths, false);
		Value a(args, false);

		Value rslt = ((txClass *) cls)->cls->call(o, t, a);

		return nt_value_incref(rslt.borrowCValue());
	}

	static void     free        (ntClass *cls) {
		delete ((txClass *) cls)->cls;
		delete ((txClass *) cls);
	}

	txClass(Class* cls) {
		this->hdr.del       = (cls->getFlags() & Class::FlagDelete)    ? txClass::del : NULL;
		this->hdr.get       = (cls->getFlags() & Class::FlagGet)       ? txClass::get : NULL;
		this->hdr.set       = (cls->getFlags() & Class::FlagSet)       ? txClass::set : NULL;
		this->hdr.enumerate = (cls->getFlags() & Class::FlagEnumerate) ? txClass::enumerate    : NULL;
		this->hdr.call      = (cls->getFlags() & Class::FlagCall)      ? txClass::call         : NULL;
		this->hdr.free      = txClass::free;
		this->cls = cls;
	}
};

Class::Flags Class::getFlags () { return Class::FlagNone; }
Value        Class::del      (Value& obj, Value& name)               { return obj.newUndefined().toException(); }
Value        Class::get      (Value& obj, Value& name)               { return obj.newUndefined().toException(); }
Value        Class::set      (Value& obj, Value& name, Value& value) { return obj.newUndefined().toException(); }
Value        Class::enumerate(Value& obj)                            { return obj.newUndefined().toException(); }
Value        Class::call     (Value& obj, Value& ths,  Value& args)  { return obj.newUndefined().toException(); }
Class::~Class() {}

Value::Value() {
	internal = NULL;
}

Value::Value(ntValue *value, bool steal) {
	if (!steal) nt_value_incref(value);
	internal = value;
}

Value::Value(const Value& value) {
	internal = value.internal;
	nt_value_incref(internal);
}

Value::~Value() {
	internal = nt_value_decref(internal);
}

Value& Value::operator=(const Value& value) {
	internal = nt_value_decref(internal);
	internal = nt_value_incref(value.internal);
	return *this;
}

Value Value::operator[](long index) {
	return get(index);
}

Value Value::operator[](Value& name) {
	return get(name);
}

Value Value::operator[](UTF8 name) {
	return get(name);
}

Value Value::operator[](UTF16 name) {
	return get(name);
}

Value Value::newBoolean(bool b) const {
	return nt_value_new_boolean(internal, b);
}

Value Value::newNumber(double n) const {
	return nt_value_new_number(internal, n);
}

Value Value::newString(UTF8 string) const {
	return nt_value_new_string_utf8_length(internal, string.data(), string.length());
}

Value Value::newString(UTF16 string) const {
	return nt_value_new_string_utf16_length(internal, string.data(), string.length());
}

Value Value::newArray(const Value* const* array) const {
	long len;
	for (len=0 ; array && array[len] ; len++);

	const ntValue **a = NULL;
	if (array) {
		a = new const ntValue*[len+1];
		for (len=0 ; array && array[len] ; len++)
			a[len] = array[len]->borrowCValue();
		a[len] = NULL;
	}

	Value v = nt_value_new_array(internal, a);

	delete[] a;
	return v;
}

Value Value::newArrayBuilder(Value item) {
	ntValue *tmp = nt_value_new_array_builder(internal, item.internal);
	if (!isArray()) return tmp;
	return Value(tmp, false);
}

Value Value::newArrayBuilder(bool item) {
	return newArrayBuilder(newBoolean(item));
}

Value Value::newArrayBuilder(int item) {
	return newArrayBuilder(newNumber(item));
}

Value Value::newArrayBuilder(long item) {
	return newArrayBuilder(newNumber(item));
}

Value Value::newArrayBuilder(double item) {
	return newArrayBuilder(newNumber(item));
}

Value Value::newArrayBuilder(const char* item) {
	return newArrayBuilder(newString(item));
}

Value Value::newArrayBuilder(const Char* item) {
	return newArrayBuilder(newString(item));
}

Value Value::newArrayBuilder(UTF8 item) {
	return newArrayBuilder(newString(item));
}

Value Value::newArrayBuilder(UTF16 item) {
	return newArrayBuilder(newString(item));
}

Value Value::newArrayBuilder(NativeFunction item) {
	return newArrayBuilder(newFunction(item));
}

Value Value::newFunction(NativeFunction func) const {
	Value f = nt_value_new_function(internal, txFunction);
	f.setPrivate("natus::Function++", (void*) func);
	return f;
}

Value Value::newObject(Class* cls) const {
	if (!cls) return nt_value_new_object(internal, NULL);
	return nt_value_new_object(internal, (ntClass*) new txClass(cls));
}

Value Value::newNull() const {
	return nt_value_new_null(internal);
}

Value Value::newUndefined() const {
	return nt_value_new_undefined(internal);
}

Value Value::getGlobal() const {
	return nt_value_get_global(internal);
}

Engine Value::getEngine() const {
	Engine e(nt_value_get_engine(internal));
	return e;
}

Value::Type Value::getType() const {
	return (Value::Type) nt_value_get_type(internal);
}

const char* Value::getTypeName() const {
	return nt_value_get_type_name(internal);
}

bool Value::borrowContext(void **context, void **value) const {
	return nt_value_borrow_context(internal, context, value);
}

ntValue* Value::borrowCValue() const {
	return internal;
}

bool Value::isGlobal() const {
	return nt_value_is_global(internal);
}

bool Value::isException() const {
	return nt_value_is_exception(internal);
}

bool Value::isType(Value::Type types) const {
	return nt_value_is_type(internal, (ntValueType) types);
}

bool Value::isArray() const {
	return nt_value_is_array(internal);
}

bool Value::isBool() const {
	return nt_value_is_bool(internal);
}

bool Value::isFunction() const {
	return nt_value_is_function(internal);
}

bool Value::isNull() const {
	return nt_value_is_null(internal);
}

bool Value::isNumber() const {
	return nt_value_is_number(internal);
}

bool Value::isObject() const {
	return nt_value_is_object(internal);
}

bool Value::isString() const {
	return nt_value_is_string(internal);
}

bool Value::isUndefined() const {
	return nt_value_is_undefined(internal);
}

Value Value::toException() {
	return Value(nt_value_to_exception(internal), false);
}

template <> bool   Value::to<bool>()   const {
	return nt_value_to_bool(internal);
}

template <> double Value::to<double>() const {
	return nt_value_to_double(internal);
}

template <> int    Value::to<int>()    const {
	return nt_value_to_int(internal);
}

template <> long   Value::to<long>()   const {
	return nt_value_to_long(internal);
}

template <> UTF8   Value::to<UTF8>()   const {
	size_t len;
	char* tmp = nt_value_to_string_utf8(internal, &len);
	if (!tmp) return UTF8();
	UTF8 rslt(tmp, len);
	free(tmp);
	return rslt;
}

template <> UTF16  Value::to<UTF16>()  const {
	size_t len;
	Char* tmp = nt_value_to_string_utf16(internal, &len);
	if (!tmp) return UTF16();
	UTF16 rslt(tmp, len);
	free(tmp);
	return rslt;
}

Value Value::del(Value idx) {
	return nt_value_del(internal, idx.internal);
}

Value Value::del(UTF8 idx) {
	Value n = newString(idx);
	return nt_value_del(internal, n.internal);
}

Value Value::del(UTF16 idx) {
	Value n = newString(idx);
	return nt_value_del(internal, n.internal);
}

Value Value::del(size_t idx) {
	return nt_value_del_index(internal, idx);
}

Value Value::delRecursive(UTF8 path) {
	return nt_value_del_recursive_utf8(internal, path.c_str());
}

Value Value::get(Value idx) const {
	return nt_value_get(internal, idx.internal);
}

Value Value::get(UTF8 idx) const {
	Value n = newString(idx);
	return nt_value_get(internal, n.internal);
}

Value Value::get(UTF16 idx) const {
	Value n = newString(idx);
	return nt_value_get(internal, n.internal);
}

Value Value::get(size_t idx) const {
	return nt_value_get_index(internal, idx);
}

Value Value::getRecursive(UTF8 path) {
	return nt_value_get_recursive_utf8(internal, path.c_str());
}

Value Value::set(Value idx, Value value, Value::PropAttr attrs) {
	return nt_value_set(internal, idx.internal, value.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, bool value, Value::PropAttr attrs) {
	Value v = newBoolean(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, int value, Value::PropAttr attrs) {
	Value v = newNumber(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, long value, Value::PropAttr attrs) {
	Value v = newNumber(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, double value, Value::PropAttr attrs) {
	Value v = newNumber(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, const char* value, Value::PropAttr attrs) {
	Value v = newString(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, const Char* value, Value::PropAttr attrs) {
	Value v = newString(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, UTF8 value, Value::PropAttr attrs) {
	Value v = newString(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, UTF16 value, Value::PropAttr attrs) {
	Value v = newString(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(Value idx, NativeFunction value, Value::PropAttr attrs) {
	Value v = newFunction(value);
	return nt_value_set(internal, idx.internal, v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, Value value, Value::PropAttr attrs) {
	Value n = newString(idx);
	return nt_value_set(internal, n.borrowCValue(), value.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, bool value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newBoolean(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, int value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, long value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, double value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, const char* value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, const Char* value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, UTF8 value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, UTF16 value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF8 idx, NativeFunction value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newFunction(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, Value value, Value::PropAttr attrs) {
	Value n = newString(idx);
	return nt_value_set(internal, n.borrowCValue(), value.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, bool value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newBoolean(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, int value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, long value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, double value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newNumber(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, const char* value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, const Char* value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, UTF8 value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, UTF16 value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newString(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(UTF16 idx, NativeFunction value, Value::PropAttr attrs) {
	Value n = newString(idx);
	Value v = newFunction(value);
	return nt_value_set(internal, n.borrowCValue(), v.internal, (ntPropAttr) attrs);
}

Value Value::set(size_t idx, Value value) {
	return nt_value_set_index(internal, idx, value.internal);
}

Value Value::set(size_t idx, bool value) {
	Value v = newBoolean(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, int value) {
	Value v = newNumber(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, long value) {
	Value v = newNumber(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, double value) {
	Value v = newNumber(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, const char* value) {
	Value v = newString(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, const Char* value) {
	Value v = newString(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, UTF8 value) {
	Value v = newString(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, UTF16 value) {
	Value v = newString(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::set(size_t idx, NativeFunction value) {
	Value v = newFunction(value);
	return nt_value_set_index(internal, idx, v.internal);
}

Value Value::setRecursive(UTF8 path, Value val, Value::PropAttr attrs, bool mkpath) {
	return nt_value_set_recursive_utf8(internal, path.c_str(), val.internal, (ntPropAttr) attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, bool val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newBoolean(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, int val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, long val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, double val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, const char* val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newString(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, const Char* val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newString(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, UTF8 val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newString(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, UTF16 val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newString(val), attrs, mkpath);
}

Value Value::setRecursive(UTF8 path, NativeFunction val, Value::PropAttr attrs, bool mkpath) {
	return setRecursive(path, newFunction(val), attrs, mkpath);
}

Value Value::enumerate() const {
	return nt_value_enumerate(internal);
}

bool Value::setPrivate(const char* key, void *priv, FreeFunction free) {
	return nt_value_private_set(internal, key, priv, free);
}

bool Value::setPrivate(const char* key, Value value) {
	return nt_value_private_set_value(internal, key, value.internal);
}

template <> void* Value::getPrivate<void*>(const char* key) const {
	return nt_value_private_get(internal, key);
}

template <> Value Value::getPrivate<Value>(const char* key) const {
	return nt_value_private_get_value(internal, key);
}

Value Value::evaluate(Value javascript, Value filename, unsigned int lineno) {
	return nt_value_evaluate(internal, javascript.internal, filename.internal, lineno);
}

Value Value::evaluate(UTF8 javascript, UTF8 filename, unsigned int lineno) {
	Value js = newString(javascript);
	Value fn = newString(filename);
	return nt_value_evaluate(internal, js.internal, fn.internal, lineno);
}

Value Value::evaluate(UTF16 javascript, UTF16 filename, unsigned int lineno) {
	Value js = newString(javascript);
	Value fn = newString(filename);
	return nt_value_evaluate(internal, js.internal, fn.internal, lineno);
}

Value Value::call(Value ths, Value args) {
	return nt_value_call(internal, ths.internal, args.internal);
}

Value Value::call(UTF8 name, Value args) {
	Value func = get(name);
	return nt_value_call(func.internal, internal, args.internal);
}

Value Value::call(UTF16 name, Value args) {
	Value func = get(name);
	return nt_value_call(func.internal, internal, args.internal);
}

Value Value::callNew(Value args) {
	return nt_value_call_new(internal, args.internal);
}

Value Value::callNew(UTF8  name, Value args) {
	Value fnc = get(name);
	return nt_value_call_new(fnc.internal, args.internal);
}

Value Value::callNew(UTF16 name, Value args) {
	Value fnc = get(name);
	return nt_value_call_new(fnc.internal, args.internal);
}
