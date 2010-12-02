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

#include <cassert>
#include <iostream>

#include <jsapi.h>

#include <natus/engine.h>
using namespace natus;

static jsval getJSValue(Value& value);
static JSBool obj_del (JSContext *cx, JSObject *obj, jsid id, jsval *vp);
static JSBool obj_get (JSContext *cx, JSObject *obj, jsid id, jsval *vp);
static JSBool obj_set (JSContext *cx, JSObject *obj, jsid id, jsval *vp);
static JSBool obj_enum(JSContext *cx, JSObject *obj, JSIterateOp enum_op, jsval *statep, jsid *idp);
static JSBool obj_call(JSContext *ctx, uintN argc, jsval *vp);
static JSBool obj_new (JSContext *ctx, uintN argc, jsval *vp);
static JSBool fnc_call(JSContext *ctx, uintN argc, jsval *vp);

static void finalize(JSContext *ctx, JSObject *obj) {
	if (JS_ObjectIsFunction(ctx, obj)) {
		jsval val;
		if (!JS_GetReservedSlot(ctx, obj, 0, &val)) return;
		obj = JSVAL_TO_OBJECT(val);
		if (!obj) return;
	}

	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JS_GetPrivate(ctx, obj);
	delete cfp;
}

static JSClass glbdef = { "global", JSCLASS_GLOBAL_FLAGS, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
		JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub, };

static JSClass stkdef = { "Object", JSCLASS_HAS_PRIVATE, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
		JS_ResolveStub, JS_ConvertStub, finalize };

static JSClass clldef = { "CallableObject", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE, JS_PropertyStub,
		obj_del, obj_get, obj_set, (JSEnumerateOp) obj_enum,
		JS_ResolveStub, JS_ConvertStub, finalize, NULL, NULL, obj_call, obj_new };

static JSClass objdef = { "Object", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE, JS_PropertyStub,
		obj_del, obj_get, obj_set, (JSEnumerateOp) obj_enum,
		JS_ResolveStub, JS_ConvertStub, finalize };

static ClassFuncPrivate* getCFP(JSContext* ctx, jsval val) {
	JSObject *obj = NULL;
	if (!JS_ValueToObject(ctx, val, &obj)) return NULL;

	jsval privval;
	if (JS_ObjectIsFunction(ctx, obj)
			&& JS_GetReservedSlot(ctx, obj, 0, &privval)
			&& JSVAL_IS_OBJECT(privval)) {
		// We have a function
		JSObject *prv;
		if (JS_ValueToObject(ctx, privval, &prv))
			obj = prv;
	}
	if (!obj) return NULL;

	return (ClassFuncPrivate *) JS_GetPrivate(ctx, obj);
}

static void report_error(JSContext *cx, const char *message,
		JSErrorReport *report) {
	fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename
			: "<no filename>", (unsigned int) report->lineno, message);
}

class SpiderMonkeyEngineValue : public EngineValue {
	friend jsval getJSValue(Value& value);
public:
	SpiderMonkeyEngineValue(JSContext* ctx) : EngineValue(this) {
		if (!ctx) throw bad_alloc();
		JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT);
		JS_SetVersion(ctx, JSVERSION_LATEST);
		JS_SetErrorReporter(ctx, report_error);

		JSObject* glbl = JS_NewObject(ctx, &glbdef, NULL, NULL);
		if (!glbl || !JS_InitStandardClasses(ctx, glbl)) throw bad_alloc();
		this->ctx = ctx;
		this->val = OBJECT_TO_JSVAL(glbl);
		(void) JSVAL_LOCK(this->ctx, this->val);
	}

	SpiderMonkeyEngineValue(EngineValue* glb, jsval val, bool exc=false) : EngineValue(glb, exc) {
		this->ctx = static_cast<SpiderMonkeyEngineValue*>(glb)->ctx;
		this->val = val;
		(void) JSVAL_LOCK(ctx, val);
	}

	virtual ~SpiderMonkeyEngineValue() {
		(void) JSVAL_UNLOCK(ctx, val);
		if (isGlobal())
			JS_DestroyContext(ctx);
	}

	virtual bool supportsPrivate() {
		return !isNull() && (isFunction() || isObject()) && getCFP(ctx, val);
	}

	virtual Value   newBool(bool b) {
		return Value(new SpiderMonkeyEngineValue(glb, BOOLEAN_TO_JSVAL(b)));
	}

	virtual Value   newNumber(double n) {
		jsval v;
		assert(JS_NewNumberValue(ctx, n, &v));
		return Value(new SpiderMonkeyEngineValue(glb, v));
	}

	virtual Value   newString(string str) {
		return Value(new SpiderMonkeyEngineValue(glb, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, str.c_str()))));
	}

	virtual Value   newArray(vector<Value> array) {
		jsval *valv = new jsval[array.size()];
		for (unsigned int i=0 ; i < array.size() ; i++)
			valv[i] = getJSValue(array[i]);
		JSObject* obj = JS_NewArrayObject(ctx, array.size(), valv);
		delete[] valv;
		return Value(new SpiderMonkeyEngineValue(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newFunction(NativeFunction func, void *priv, FreeFunction free) {
		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = NULL;
		cfp->func = func;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		JSFunction *fnc = JS_NewFunction(ctx, fnc_call, 0, JSFUN_CONSTRUCTOR, NULL, NULL);
		JSObject   *obj = JS_GetFunctionObject(fnc);
		JSObject   *prv = JS_NewObject(ctx, &stkdef, NULL, NULL);
		if (!fnc || !obj || !prv) return newUndefined();

		if (!JS_SetPrivate(ctx, prv, cfp) || !JS_SetReservedSlot(ctx, obj, 0, OBJECT_TO_JSVAL(prv))) {
			if (cfp->free && cfp->priv)
				cfp->free(cfp->priv);
			delete cfp;
			return newUndefined();
		}

		return Value(new SpiderMonkeyEngineValue(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newObject(Class* cls, void* priv, FreeFunction free) {
		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = cls;
		cfp->func = NULL;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		// Pick what type of object to build
		JSClass* clsp;
		if (!cls)
			clsp = &stkdef; // Stock object, just a finalizer
		else if (cls->getFlags() & Class::Callable)
			clsp = &clldef; // Full class, all methods
		else if (cls->getFlags() & Class::Object)
			clsp = &objdef; // Property methods only
		else
			assert(false);

		// Build the object
		JSObject *obj = JS_NewObject(ctx, clsp, NULL, NULL);
		if (!obj || !JS_SetPrivate(ctx, obj, cls ? cfp : NULL)) {
			delete cfp;
			return newUndefined();
		} else if (!cls)
			delete cfp;

		return Value(new SpiderMonkeyEngineValue(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newNull() {
		return Value(new SpiderMonkeyEngineValue(glb, JSVAL_NULL));
	}

	virtual Value   newUndefined() {
		return Value(new SpiderMonkeyEngineValue(glb, JSVAL_VOID));
	}

	virtual Value   evaluate(string jscript, string filename, unsigned int lineno=0, bool shift=false) {
		jsval val;
		if (JS_EvaluateScript(ctx, shift ? toJSObject() : JS_GetGlobalObject(ctx), jscript.c_str(), jscript.length(), filename.c_str(), lineno, &val) == JS_FALSE) {
			if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &val))
				return Value(new SpiderMonkeyEngineValue(glb, val)).toException();
			return newUndefined();
		}
		return Value(new SpiderMonkeyEngineValue(glb, val));
	}

	virtual bool    isGlobal() {
		return JS_GetGlobalObject(ctx) == toJSObject();
	}

	virtual bool    isArray() {
		if (!JSVAL_IS_OBJECT(val)) return false;
		return !isNull() && JS_IsArrayObject(ctx, toJSObject());
	}

	virtual bool    isBool() {
		return JSVAL_IS_BOOLEAN(val);
	}

	virtual bool    isFunction() {
		if (!JSVAL_IS_OBJECT(val)) return false;
		return !isNull() && JS_ObjectIsFunction(ctx, toJSObject());
	}

	virtual bool    isNull() {
		return JSVAL_IS_NULL(val);
	}

	virtual bool    isNumber() {
		return JSVAL_IS_NUMBER(val);
	}

	virtual bool    isObject() {
		return !isNull() && JSVAL_IS_OBJECT(val) && !isArray() && !isFunction();
	}

	virtual bool    isString() {
		return JSVAL_IS_STRING(val);
	}

	virtual bool    isUndefined() {
		return JSVAL_IS_VOID(val);
	}

	virtual bool    toBool() {
		return JSVAL_TO_BOOLEAN(val);
	}

	virtual double  toDouble() {
		double d;
		JS_ValueToNumber(ctx, val, &d);
		return d;
	}

	virtual string  toString() {
		JSString *s = NULL;

		if (JSVAL_IS_OBJECT(val)) {
			Value x = this->call(this->get("toString"), vector<Value>());
			if (x.isString()) {
				s = JS_ValueToString(ctx, getJSValue(x));
			}
		}
		if (!s)
			s = JS_ValueToString(ctx, val);
		assert(s);
		string foo = string(JS_GetStringBytes(s));

		return foo;
	}

	virtual bool    del(string name) {
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->delProperty
				||  jsc->delProperty == JS_PropertyStub
				|| !jsc->delProperty(ctx, toJSObject(), toJSID(name), NULL))
			return JS_DeleteProperty(ctx, toJSObject(), name.c_str());
		return true;
	}

	virtual bool    del(long idx) {
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->delProperty
				||  jsc->delProperty == JS_PropertyStub
				|| !jsc->delProperty(ctx, toJSObject(), toJSID(idx), NULL))
			return JS_DeleteElement(ctx, toJSObject(), idx);
		return true;
	}

	virtual Value   get(string name) {
		jsval v = JSVAL_VOID;
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->getProperty
				||  jsc->getProperty == JS_PropertyStub
				|| !jsc->getProperty(ctx, toJSObject(), toJSID(name), &v))
			JS_GetProperty(ctx, toJSObject(), name.c_str(), &v);
		return Value(new SpiderMonkeyEngineValue(glb, v));
	}

	virtual Value   get(long idx) {
		jsval val = JSVAL_VOID;
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->getProperty
				||  jsc->getProperty == JS_PropertyStub
				|| !jsc->getProperty(ctx, toJSObject(), toJSID(idx), &val))
			JS_GetElement(ctx, toJSObject(), idx, &val);
		return Value(new SpiderMonkeyEngineValue(glb, val));
	}

	virtual bool    set(string name, Value value, Value::PropAttrs attrs) {
		jsint  flags  = (attrs & Value::DontEnum)   ? 0 : JSPROP_ENUMERATE;
		       flags |= (attrs & Value::ReadOnly)   ? JSPROP_READONLY  : 0;
		       flags |= (attrs & Value::DontDelete) ? JSPROP_PERMANENT : 0;

		jsval  v = getJSValue(value);
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->setProperty
				||  jsc->setProperty == JS_PropertyStub
				|| !jsc->setProperty(ctx, toJSObject(), toJSID(name), &v))
			JS_SetProperty(ctx, toJSObject(), name.c_str(), &v);

		JSBool found;
		return JS_SetPropertyAttributes(ctx, toJSObject(), name.c_str(), flags, &found);
	}

	virtual bool    set(long idx, Value value) {
		jsval  v = getJSValue(value);
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->setProperty
				||  jsc->setProperty == JS_PropertyStub
				|| !jsc->setProperty(ctx, toJSObject(), toJSID(idx), &v))
			return JS_SetElement(ctx, toJSObject(), idx, &v);
		return true;
	}

	virtual bool    setPrivate(void *priv) {
		ClassFuncPrivate *cfp = getCFP(ctx, val);
		if (!cfp) return false;
		cfp->priv = priv;
		return true;
	}

	virtual void*   getPrivate() {
		ClassFuncPrivate *cfp = getCFP(ctx, val);
		if (!cfp) return NULL;
		return cfp->priv;
	}

	virtual Value   call(Value func, vector<Value> args) {
		// Convert to jsval array
		jsval *argv = new jsval[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[0]);

		// Call the function
		jsval rval;
		bool  exception = false;
		if (!JS_CallFunctionValue(ctx, toJSObject(), getJSValue(func), args.size(), argv, &rval)) {
			if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &rval))
				exception = true;
			else {
				delete[] argv;
				return newUndefined();
			}
		}
		delete[] argv;

		Value v = Value(new SpiderMonkeyEngineValue(glb, rval));
		return exception ? v.toException() : v;

	}

	virtual Value   callNew(vector<Value> args) {
		// Convert to jsval array
		jsval *argv = new jsval[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[0]);

		// Call the function
		jsval rval;
		bool  exception = false;

		JSObject *obj = JS_New(ctx, toJSObject(), args.size(), argv);
		if (!obj && JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &rval))
			exception = true;
		else if (obj)
			rval = OBJECT_TO_JSVAL(obj);
		else {
			delete[] argv;
			return newUndefined();
		}

		Value v = Value(new SpiderMonkeyEngineValue(glb, rval));
		return exception ? v.toException() : v;
	}

private:
	JSContext *ctx;
	jsval      val;

	JSObject* toJSObject() {
		return JSVAL_TO_OBJECT(val);
	}

	jsid toJSID(string key) {
		jsid vid;
		JS_ValueToId(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, key.c_str())), &vid);
		return vid;
	}

	jsid toJSID(double key) {
		jsid vid;
		jsval rval;
		JS_NewNumberValue(ctx, key, &rval);
		JS_ValueToId(ctx, rval, &vid);
		return vid;
	}

};

static jsval getJSValue(Value& value) {
	return SpiderMonkeyEngineValue::borrowInternal<SpiderMonkeyEngineValue>(value)->val;
}

static JSBool obj_del(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	ClassFuncPrivate *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss)
		return JS_FALSE;

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(new SpiderMonkeyEngineValue(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(new SpiderMonkeyEngineValue(cfp->glbl, key));

	if (vkey.isString())
		return cfp->clss->del(obj, vkey.toString());
	//else if (vkey.isNumber())
	//	return cfp->clss->del(obj, vkey.toLong());
	assert(false);
}

static JSBool obj_get(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	ClassFuncPrivate *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(new SpiderMonkeyEngineValue(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(new SpiderMonkeyEngineValue(cfp->glbl, key));
	Value res;
	if (vkey.isString())
		res = cfp->clss->get(obj, vkey.toString());
	else if (vkey.isNumber())
		res = cfp->clss->get(obj, vkey.toLong());
	else
		assert(false);

	*vp = getJSValue(res);
	return JS_TRUE;
}

static JSBool obj_set(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	ClassFuncPrivate *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(new SpiderMonkeyEngineValue(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(new SpiderMonkeyEngineValue(cfp->glbl, key));
	Value val  = Value(new SpiderMonkeyEngineValue(cfp->glbl, *vp));
	if (vkey.isString())
		return cfp->clss->set(obj, vkey.toString(), val);
	else if (vkey.isNumber())
		return cfp->clss->set(obj, vkey.toLong(), val);
	else
		assert(false);
}

static JSBool obj_enum(JSContext *ctx, JSObject *object, JSIterateOp enum_op, jsval *statep, jsid *idp) {
	uint len;
	jsval step;
	if (enum_op == JSENUMERATE_NEXT) {
		assert(statep && idp);
		if (!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(*statep), &len)) return JS_FALSE;
		if (!JS_GetProperty(ctx, JSVAL_TO_OBJECT(*statep), "step", &step)) return JS_FALSE;
		if ((uint) JSVAL_TO_INT(step) < len) {
			jsval item;
			if (!JS_GetElement(ctx, JSVAL_TO_OBJECT(*statep), JSVAL_TO_INT(step), &item)) return JS_FALSE;
			*idp = item;
		} else {
			*idp = JSVAL_NULL;
		}
		return JS_TRUE;
	} else if (enum_op == JSENUMERATE_DESTROY) {
		assert(statep);
		(void) JSVAL_UNLOCK(ctx, *statep);
		return JS_TRUE;
	}
	assert(enum_op == JSENUMERATE_INIT);
	assert(statep);

	ClassFuncPrivate *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	// Call our callback
	Value obj = Value(new SpiderMonkeyEngineValue(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value res = cfp->clss->enumerate(obj);

	// Handle the results
	step = INT_TO_JSVAL(0);
	if (!res.isArray() ||
		!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(getJSValue(res)), &len) ||
		!JS_SetProperty(ctx, JSVAL_TO_OBJECT(getJSValue(res)), "step", &step)) {
		return JS_FALSE;
	}

	// Keep the jsval, but dispose of the ntValue
	*statep = getJSValue(res);
	if (idp) *idp = INT_TO_JSVAL(len);
	return JS_TRUE;
}

static inline JSBool int_call(JSContext *ctx, uintN argc, jsval *vp, bool constr) {
	ClassFuncPrivate *cfp = getCFP(ctx, JS_CALLEE(ctx, vp));
	if (!cfp || (!cfp->func && !cfp->clss)){
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	// Allocate our array of arguments
	vector<Value> args = vector<Value>();
	for (unsigned int i=0; i < argc ; i++)
		args.push_back(Value(new SpiderMonkeyEngineValue(cfp->glbl, JS_ARGV(ctx, vp)[i])));

	// Make the call
	Value fnc = Value(new SpiderMonkeyEngineValue(cfp->glbl, JS_CALLEE(ctx, vp)));
	Value ths = constr ? fnc.newUndefined() : Value(new SpiderMonkeyEngineValue(cfp->glbl, JS_THIS(ctx, vp)));
	Value res = cfp->clss
					? (constr
						? cfp->clss->callNew(fnc, args)
						: cfp->clss->call(fnc, args))
					: cfp->func(ths, fnc, args, cfp->priv);

	// Handle results
	if (res.isException()) {
		JS_SetPendingException(ctx, getJSValue(res));
		return JS_FALSE;
	}
	JS_SET_RVAL(ctx, vp, getJSValue(res));
	return JS_TRUE;
}

static JSBool obj_call(JSContext *ctx, uintN argc, jsval *vp) {
	return int_call(ctx, argc, vp, false);
}

static JSBool obj_new(JSContext *ctx, uintN argc, jsval *vp) {
	return int_call(ctx, argc, vp, true);
}

static JSBool fnc_call(JSContext *ctx, uintN argc, jsval *vp) {
	return int_call(ctx, argc, vp, JS_IsConstructing(ctx, vp));
}

static EngineValue* engine_newg(void *engine) {
	return new SpiderMonkeyEngineValue(JS_NewContext((JSRuntime*) engine, 1024 * 1024));
}

static void engine_free(void *engine) {
	JS_DestroyRuntime((JSRuntime*) engine);
	JS_ShutDown();
}

static void *engine_init() {
	return JS_NewRuntime(1024 * 1024);
}

NATUS_ENGINE("SpiderMonkey", "JS_GetProperty", engine_init, engine_newg, engine_free);
