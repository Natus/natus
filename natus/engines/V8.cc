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

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <new>
#include <cstdio>

#include <natus/engine.h>
using namespace natus;

#include <v8.h>

class V8EngineValue;

class V8Class {
public:
	v8::Persistent<v8::Object> data;
	static v8::Handle<v8::Value>   item_get(uint32_t index, const v8::AccessorInfo& info);
	static v8::Handle<v8::Value>   property_get(v8::Local<v8::String> property, const v8::AccessorInfo& info);
	static v8::Handle<v8::Boolean> item_del(uint32_t index, const v8::AccessorInfo& info);
	static v8::Handle<v8::Boolean> property_del(v8::Local<v8::String> property, const v8::AccessorInfo& info);
	static v8::Handle<v8::Value>   item_set(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
	static v8::Handle<v8::Value>   property_set(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
	static v8::Handle<v8::Array>   enumerate(const v8::AccessorInfo& info);
	static v8::Handle<v8::Value>   call(const v8::Arguments& args);
	V8Class(v8::Handle<v8::Context> ctx, ClassFuncPrivate* cfp);
	~V8Class();

private:
	ClassFuncPrivate* cfp;
	static void finalize(v8::Persistent<v8::Value> object, void *parameter);
};

struct shft {
	static v8::Handle<v8::Value> get(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		return info.Data()->ToObject()->Get(property);
	}

	static v8::Handle<v8::Value> set(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
		if (info.Data()->ToObject()->Set(property, value))
			return value;
		return v8::Handle<v8::Value>();
	}

	static v8::Handle<v8::Boolean> del(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		return info.Data()->ToObject()->Delete(property) ? v8::True() : v8::False();
	}

	static v8::Handle<v8::Array> enumerate(const v8::AccessorInfo& info) {
		return info.Data()->ToObject()->GetPropertyNames();
	}
};

static v8::Handle<v8::Value> getJSValue(Value& value);

string getString(v8::Handle<v8::Context> ctx, v8::Handle<v8::String> str) {
	v8::HandleScope hs;
	v8::Context::Scope cs(ctx);
	char *buffer = new char[str->Utf8Length()];
	str->WriteUtf8(buffer, str->Utf8Length());
	string output = string(buffer, str->Utf8Length());
	delete[] buffer;
	return output;
}

class V8EngineValue : public EngineValue {
	friend v8::Handle<v8::Value> getJSValue(Value& value);
	friend class V8Class;
public:
	V8EngineValue(v8::Handle<v8::Context> ctx) : EngineValue(this) {
		v8::HandleScope hs;
		this->glb = this;
		this->ctx = v8::Persistent<v8::Context>::New(ctx);
		this->val = v8::Persistent<v8::Value>::New(ctx->Global());
	}

	V8EngineValue(EngineValue* glb, v8::Handle<v8::Value> val, bool exc=false) : EngineValue(glb, exc) {
		v8::HandleScope hs;
		this->ctx = v8::Persistent<v8::Context>::New(static_cast<V8EngineValue*>(glb)->ctx);
		this->val = v8::Persistent<v8::Value>::New(val);
	}

	virtual ~V8EngineValue() {
		val.Dispose();
		ctx.Dispose();
	}

	virtual bool supportsPrivate() {
		v8::HandleScope hs;
		if (!val->IsObject()) return false;

		v8::Handle<v8::Value>  val = this->val->ToObject()->GetHiddenValue(v8::String::New("__internal__"));
		if (val.IsEmpty()) return false;

		return val->ToObject()->GetPointerFromInternalField(0) != NULL;
	}

	virtual Value   newBool(bool b) {
		v8::HandleScope hs;
		return Value(new V8EngineValue(glb, v8::Boolean::New(b)));
	}

	virtual Value   newNumber(double n) {
		v8::HandleScope hs;
		return Value(new V8EngineValue(glb, v8::Number::New(n)));
	}

	virtual Value   newString(string str) {
		v8::HandleScope hs;
		return Value(new V8EngineValue(glb, v8::String::New(str.c_str())));
	}

	virtual Value   newArray(vector<Value> array) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		v8::Handle<v8::Array> valv = v8::Array::New(array.size());
		for (unsigned int i=0 ; i < array.size() ; i++)
			valv->Set(i, getJSValue(array[i]));
		return Value(new V8EngineValue(glb, valv));
	}

	virtual Value   newFunction(NativeFunction func, void *priv, FreeFunction free) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = NULL;
		cfp->func = func;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		V8Class *v8cls = new V8Class(ctx, cfp);
		v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(V8Class::call, v8cls->data);
		v8::Handle<v8::Function>        fnc = ft->GetFunction();
		fnc->SetHiddenValue(v8::String::New("__internal__"), v8cls->data);
		return Value(new V8EngineValue(glb, fnc));
	}

	virtual Value   newObject(Class* cls, void* priv, FreeFunction free) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = cls;
		cfp->func = NULL;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		v8::Handle<v8::ObjectTemplate> ot = v8::ObjectTemplate::New();
		V8Class *v8cls = new V8Class(ctx, cfp);
		if (cls) {
			if (cls->getFlags() & Class::Object) {
				ot->SetNamedPropertyHandler(V8Class::property_get,
						                    V8Class::property_set, NULL,
						                    V8Class::property_del,
						                    V8Class::enumerate, v8cls->data);
				ot->SetIndexedPropertyHandler(V8Class::item_get,
											  V8Class::item_set, NULL,
											  V8Class::item_del,
											  V8Class::enumerate, v8cls->data);
			}

			if (cls->getFlags() & Class::Function) {
				ot->SetCallAsFunctionHandler(V8Class::call, v8cls->data);
				v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(V8Class::call, v8cls->data);
				v8::Handle<v8::Function> fnc = ft->GetFunction();
				v8::Handle<v8::Object> obj = ot->NewInstance();
				fnc->SetHiddenValue(v8::String::New("__internal__"), v8cls->data);
				obj->SetHiddenValue(v8::String::New("__internal__"), v8cls->data);
				obj->SetHiddenValue(v8::String::New("__function__"), fnc);
				fnc->SetHiddenValue(v8::String::New("__object__"),   obj);
				return Value(new V8EngineValue(glb, obj));
			}
		}

		v8::Handle<v8::Object> obj = ot->NewInstance();
		obj->SetHiddenValue(v8::String::New("__internal__"), v8cls->data);
		return Value(new V8EngineValue(glb, obj));
	}

	virtual Value   newNull() {
		v8::HandleScope hs;

		return Value(new V8EngineValue(glb, v8::Null()));
	}

	virtual Value   newUndefined() {
		v8::HandleScope hs;

		return Value(new V8EngineValue(glb, v8::Undefined()));
	}

	virtual Value   evaluate(string jscript, string filename, unsigned int lineno=0, bool shift=false) {
		v8::HandleScope hs;

		v8::Persistent<v8::Context> context = ctx;
		if (shift) {
			// Create our new context (and the global object which will proxy values back into our main object)
			v8::Handle<v8::ObjectTemplate> ot = v8::ObjectTemplate::New();
			ot->SetNamedPropertyHandler(shft::get, shft::set, NULL, shft::del, shft::enumerate, val);
			context = v8::Context::New(NULL, ot);
		}

		// Execute the script
		context->Enter();
		v8::TryCatch tc;
		v8::ScriptOrigin           so = v8::ScriptOrigin(v8::String::New(filename.c_str()), v8::Integer::New(lineno));
		v8::Handle<v8::String> source = v8::String::New(jscript.c_str());
		v8::Handle<v8::Script> script = filename != "" ? v8::Script::Compile(source, &so) : v8::Script::Compile(source);
		v8::Handle<v8::Value>     res = script->Run();
		context->Exit();
		context.Dispose();

		if (tc.HasCaught())
			res = tc.Exception();
		return Value(new V8EngineValue(glb, res, tc.HasCaught()));
	}

	virtual bool    isGlobal() {
		return ctx->Global() == val;
	}

	virtual bool    isArray() {
		return val->IsArray();
	}

	virtual bool    isBool() {
		return val->IsBoolean();
	}

	virtual bool    isFunction() {
		return val->IsFunction();
	}

	virtual bool    isNull() {
		return val->IsNull();
	}

	virtual bool    isNumber() {
		return val->IsNumber() || val->IsInt32() || val->IsUint32();
	}

	virtual bool    isObject() {
		return (val->IsObject() || val->IsDate() || val->IsRegExp()) && !isFunction();
	}

	virtual bool    isString() {
		return val->IsString();
	}

	virtual bool    isUndefined() {
		return val->IsUndefined();
	}

	virtual bool    toBool() {
		return val->BooleanValue();
	}

	virtual double  toDouble() {
		return val->NumberValue();
	}

	virtual string  toString() {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		if (isObject()) {
			Value toStr = this->call(this->get("toString"), vector<Value>());
			if (toStr.isString())
				return toStr.toString();
		}

		v8::TryCatch tc;
		v8::Handle<v8::String> str = val->ToString();
		if (tc.HasCaught()) {
			assert(val->IsObject());
			str = v8::String::New("[object NativeObject]");
		}
		return getString(ctx, str);
	}

	virtual bool    del(string name) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		return val->ToObject()->Delete(v8::String::New(name.c_str()));
	}

	virtual bool    del(long idx) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		return val->ToObject()->Delete(idx);
	}

	virtual Value   get(string name) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		return Value(new V8EngineValue(glb, val->ToObject()->Get(v8::String::New(name.c_str()))));
	}

	virtual Value   get(long idx) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		return Value(new V8EngineValue(glb, val->ToObject()->Get(idx)));
	}

	virtual bool    set(string name, Value value, BaseValue::PropAttrs attrs) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		// Make sure that v8 doesn't change its enum values
		assert(v8::None       == (v8::PropertyAttribute) None);
		assert(v8::ReadOnly   == (v8::PropertyAttribute) ReadOnly);
		assert(v8::DontEnum   == (v8::PropertyAttribute) DontEnum);
		assert(v8::DontDelete == (v8::PropertyAttribute) DontDelete);
		return val->ToObject()->Set(v8::String::New(name.c_str()), getJSValue(value), (v8::PropertyAttribute) attrs);
	}

	virtual bool    set(long idx, Value value) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);
		return val->ToObject()->Set(idx, getJSValue(value));
	}

	virtual bool    setPrivate(void *priv) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		v8::Handle<v8::Value>  val = this->val->ToObject()->GetHiddenValue(v8::String::New("__internal__"));
		if (val.IsEmpty()) return false;

		ClassFuncPrivate *cfp = (ClassFuncPrivate *) val->ToObject()->GetPointerFromInternalField(0);
		if (cfp) cfp->priv = priv;
		return true;
	}

	virtual void*   getPrivate() {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		v8::Handle<v8::Value>  val = this->val->ToObject()->GetHiddenValue(v8::String::New("__internal__"));
		if (val.IsEmpty()) return NULL;

		ClassFuncPrivate *cfp = (ClassFuncPrivate *) val->ToObject()->GetPointerFromInternalField(0);
		if (cfp) return cfp->priv;
		return NULL;
	}

	virtual Value   call(Value func, vector<Value> args) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		// Convert arguments
		v8::Handle<v8::Value>* argv = new v8::Handle<v8::Value>[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[0]);

		// Get the function object
		v8::Local<v8::Function> fnc;
		if (func.isObject()) {
			v8::Handle<v8::Value> intfunc = getJSValue(func)->ToObject()->GetHiddenValue(v8::String::New("__function__"));
			assert(!intfunc.IsEmpty());
			assert(intfunc->IsFunction());
			fnc = v8::Function::Cast(*intfunc);
		} else if (func.isFunction())
			fnc = v8::Function::Cast(*getJSValue(func));
		else
			return newUndefined();

		// Call it
		v8::TryCatch tc;
		v8::Handle<v8::Object> ths = val->ToObject();
		v8::Handle<v8::Value>  res = fnc->Call(ths, args.size(), argv);
		delete[] argv;

		// Handle the results
		if (res.IsEmpty())
			return Value(new V8EngineValue(glb, tc.Exception(), true));
		return Value(new V8EngineValue(glb, res));
	}

	virtual Value   callNew(vector<Value> args) {
		v8::HandleScope hs;
		v8::Context::Scope cs(ctx);

		// Convert arguments
		v8::Handle<v8::Value>* argv = new v8::Handle<v8::Value>[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[0]);

		// Get the function object
		v8::Local<v8::Function> fnc;
		if (isObject()) {
			v8::Local<v8::Value> intfunc = val->ToObject()->GetHiddenValue(v8::String::New("__function__"));
			assert(!intfunc.IsEmpty());
			assert(intfunc->IsFunction());
			fnc = v8::Function::Cast(*intfunc);
		} else
			fnc = v8::Function::Cast(*val);

		// Call it
		v8::TryCatch tc;
		v8::Handle<v8::Object> ths = val->ToObject();
		v8::Handle<v8::Value>  res = fnc->NewInstance(args.size(), argv);
		delete[] argv;

		// Handle the results
		if (res.IsEmpty())
			return Value(new V8EngineValue(glb, tc.Exception(), true));
		return Value(new V8EngineValue(glb, res));
	}

private:
	v8::Persistent<v8::Context> ctx;
	v8::Persistent<v8::Value>   val;
};

static v8::Handle<v8::Value> getJSValue(Value& value) {
	return V8EngineValue::borrowInternal<V8EngineValue>(value)->val;
}

v8::Handle<v8::Value> V8Class::item_get(uint32_t index, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	Value res = cfp->clss->get(obj, index);
	return getJSValue(res);
}

v8::Handle<v8::Value> V8Class::property_get(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	Value res = cfp->clss->get(obj, getString(static_cast<V8EngineValue*>(cfp->glbl)->ctx, property));
	return getJSValue(res);
}

v8::Handle<v8::Boolean> V8Class::item_del(uint32_t index, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	return v8::Boolean::New(cfp->clss->del(obj, index));
}

v8::Handle<v8::Boolean> V8Class::property_del(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	return v8::Boolean::New(cfp->clss->del(obj, getString(static_cast<V8EngineValue*>(cfp->glbl)->ctx, property)));
}

v8::Handle<v8::Value> V8Class::item_set(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	Value val = Value(new V8EngineValue(cfp->glbl, value));
	return v8::Boolean::New(cfp->clss->set(obj, index, val));
}

v8::Handle<v8::Value> V8Class::property_set(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	Value val = Value(new V8EngineValue(cfp->glbl, value));
	return v8::Boolean::New(cfp->clss->set(obj, getString(static_cast<V8EngineValue*>(cfp->glbl)->ctx, property), val));
}

v8::Handle<v8::Array> V8Class::enumerate(const v8::AccessorInfo& info) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(0);

	Value obj = Value(new V8EngineValue(cfp->glbl, info.This()));
	Value val = cfp->clss->enumerate(obj);
	if (!val.isArray())
		return v8::Handle<v8::Array>();
	return v8::Handle<v8::Array>(v8::Array::Cast(*getJSValue(val)));
}

v8::Handle<v8::Value> V8Class::call(const v8::Arguments& args) {
	v8::HandleScope hs;
	ClassFuncPrivate* cfp = (ClassFuncPrivate*) args.Data()->ToObject()->GetPointerFromInternalField(0);
	v8::Context::Scope cs(static_cast<V8EngineValue*>(cfp->glbl)->ctx);

	// Get the object
	v8::Local<v8::String> object = v8::String::New("__object__");
	Value obj = Value(new V8EngineValue(cfp->glbl, args.This()));
	if (cfp->clss && !args.Callee()->GetHiddenValue(object).IsEmpty())
		obj = Value(new V8EngineValue(cfp->glbl, args.Callee()->GetHiddenValue(object)->ToObject()));

	// Convert the arguments
	vector<Value> argv = vector<Value>();
	for (int i=0 ; i < args.Length() ; i++)
		argv.push_back(Value(new V8EngineValue(cfp->glbl, args[i])));

	// Do the call
	Value res;
	if (cfp->clss && args.IsConstructCall())
		res = cfp->clss->callNew(obj, argv);
	else if (cfp->clss)
		res = cfp->clss->call(obj, argv);
	else {
		Value func = Value(new V8EngineValue(cfp->glbl, args.Callee()));
		if (args.IsConstructCall())
			obj = obj.newUndefined();
		res = cfp->func(obj, func, argv, cfp->priv);
	}
	if (res.isException())
		return v8::ThrowException(getJSValue(res));
	return getJSValue(res);
}

V8Class::V8Class(v8::Handle<v8::Context> ctx, ClassFuncPrivate* cfp) {
	v8::HandleScope hs;
	v8::Context::Scope cs(ctx);
	this->cfp = cfp;

	v8::Local<v8::ObjectTemplate> ot = v8::ObjectTemplate::New();
	ot->SetInternalFieldCount(2);
	data = v8::Persistent<v8::Object>::New(ot->NewInstance());
	data->SetPointerInInternalField(0, cfp);
	data.MakeWeak(this, finalize);
}

V8Class::~V8Class() {
	delete cfp;
	data.Dispose();
	data.Clear();
}

void V8Class::finalize(v8::Persistent<v8::Value> object, void *parameter) {
	delete ((V8Class *) parameter);
}

static void engine_free(void *engine) {
	delete (char *) engine;
}

static EngineValue* engine_newg(void *engine) {
	return new V8EngineValue(v8::Context::New());
}

static void* engine_init() {
	return new char;
}

// V8_Fatal appears to be the only v8 unique extern "C" symbol
// Let's hope it doesn't get removed...
NATUS_ENGINE("V8", "V8_Fatal", engine_init, engine_newg, engine_free);
