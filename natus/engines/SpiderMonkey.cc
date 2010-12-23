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

#include <cassert>
#include <cstring>

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

static JSBool convert(JSContext *ctx, JSObject *obj, JSType type, jsval *vp) {
	jsid vid;
	JS_ValueToId(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "toString")), &vid);

	jsval v = JSVAL_VOID;
	JSClass* jsc = NULL;
	if ((((jsc = JS_GetClass(ctx, obj))
			&& jsc->getProperty
			&& jsc->getProperty != JS_PropertyStub
			&& jsc->getProperty(ctx, obj, vid, &v))
			|| JS_GetProperty(ctx, obj, "toString", &v))
			&& JS_CallFunctionValue(ctx, obj, v, 0, NULL, &v)) {
		*vp = v;
		return JS_TRUE;
	}
	return JS_FALSE;
}

class CFP : public ClassFuncPrivate {
public:
	JSClass smcls;

	CFP(EngineValue* glbl, Class* clss) : ClassFuncPrivate(glbl, clss) {
		memset(&smcls, 0, sizeof(JSClass));
		smcls.name        = "Object";
		smcls.flags       = JSCLASS_HAS_PRIVATE;
		smcls.addProperty = JS_PropertyStub;
		smcls.delProperty = JS_PropertyStub;
		smcls.getProperty = JS_PropertyStub;
		smcls.setProperty = JS_PropertyStub;
		smcls.enumerate   = JS_EnumerateStub;
		smcls.resolve     = JS_ResolveStub;
		smcls.convert     = JS_ConvertStub;
		smcls.finalize    = JS_FinalizeStub;
		smcls.convert     = convert;
	}

	CFP(EngineValue* glbl, NativeFunction func) : ClassFuncPrivate(glbl, func) {
		memset(&smcls, 0, sizeof(JSClass));
		smcls.name        = "Object";
		smcls.flags       = JSCLASS_HAS_PRIVATE;
		smcls.addProperty = JS_PropertyStub;
		smcls.delProperty = JS_PropertyStub;
		smcls.getProperty = JS_PropertyStub;
		smcls.setProperty = JS_PropertyStub;
		smcls.enumerate   = JS_EnumerateStub;
		smcls.resolve     = JS_ResolveStub;
		smcls.convert     = JS_ConvertStub;
		smcls.finalize    = JS_FinalizeStub;
		smcls.convert     = convert;
	}
};

static void finalize(JSContext *ctx, JSObject *obj) {
	if (JS_ObjectIsFunction(ctx, obj)) {
		jsval val;
		if (!JS_GetReservedSlot(ctx, obj, 0, &val)) return;
		obj = JSVAL_TO_OBJECT(val);
		if (!obj) return;
	}

	CFP *cfp = (CFP *) JS_GetPrivate(ctx, obj);
	delete cfp;
}

static JSClass glbdef = { "global", JSCLASS_GLOBAL_FLAGS, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
		JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub, };

static CFP* getCFP(JSContext* ctx, jsval val) {
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

	return (CFP *) JS_GetPrivate(ctx, obj);
}

static void report_error(JSContext *cx, const char *message,
		JSErrorReport *report) {
	fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename
			: "<no filename>", (unsigned int) report->lineno, message);
}

class SpiderMonkeyEngineValue : public EngineValue {
	friend jsval getJSValue(Value& value);
public:
	static EngineValue* getInstance(EngineValue* glb, jsval val, bool exc=false)  {
		if (JS_GetGlobalObject(static_cast<SpiderMonkeyEngineValue*>(glb)->ctx) == JSVAL_TO_OBJECT(val))
			return glb;
		return new SpiderMonkeyEngineValue(glb, val, exc);
	}

	SpiderMonkeyEngineValue(JSContext* ctx) : EngineValue(this) {
		if (!ctx) throw bad_alloc();
		JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_XML);
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
		return Value(SpiderMonkeyEngineValue::getInstance(glb, BOOLEAN_TO_JSVAL(b)));
	}

	virtual Value   newNumber(double n) {
		jsval v;
		assert(JS_NewNumberValue(ctx, n, &v));
		return Value(SpiderMonkeyEngineValue::getInstance(glb, v));
	}

	virtual Value   newString(string str) {
		return Value(SpiderMonkeyEngineValue::getInstance(glb, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, str.c_str()))));
	}

	virtual Value   newArray(vector<Value> array) {
		jsval *valv = new jsval[array.size()];
		for (unsigned int i=0 ; i < array.size() ; i++)
			valv[i] = getJSValue(array[i]);
		JSObject* obj = JS_NewArrayObject(ctx, array.size(), valv);
		delete[] valv;
		return Value(SpiderMonkeyEngineValue::getInstance(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newFunction(NativeFunction func) {
		CFP *cfp = new CFP(glb, func);
		cfp->smcls.finalize = finalize;
		cfp->smcls.name     = "NativeFunctionInternal";

		JSFunction *fnc = JS_NewFunction(ctx, fnc_call, 0, JSFUN_CONSTRUCTOR, NULL, NULL);
		JSObject   *obj = JS_GetFunctionObject(fnc);
		JSObject   *prv = JS_NewObject(ctx, &cfp->smcls, NULL, NULL);
		if (!fnc || !obj || !prv) return newUndefined();

		if (!JS_SetPrivate(ctx, prv, cfp) || !JS_SetReservedSlot(ctx, obj, 0, OBJECT_TO_JSVAL(prv))) {
			delete cfp;
			return newUndefined().toException();
		}
		return Value(SpiderMonkeyEngineValue::getInstance(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newObject(Class* cls) {
		CFP *cfp = new CFP(glb, cls);
		cfp->smcls.finalize = finalize;
		if (cls) {
			Class::Flags flags = cls->getFlags();
			cfp->smcls.name        = flags & Class::FlagFunction  ? "NativeCallable" : "NativeObject";
			cfp->smcls.flags       = flags & Class::FlagEnumerate ? JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE : JSCLASS_HAS_PRIVATE;
			cfp->smcls.delProperty = flags & Class::FlagDelete    ? obj_del  : JS_PropertyStub;
			cfp->smcls.getProperty = flags & Class::FlagGet       ? obj_get  : JS_PropertyStub;
			cfp->smcls.setProperty = flags & Class::FlagSet       ? obj_set  : JS_PropertyStub;
			cfp->smcls.enumerate   = flags & Class::FlagEnumerate ? (JSEnumerateOp) obj_enum : JS_EnumerateStub;
			cfp->smcls.call        = flags & Class::FlagCall      ? obj_call : NULL;
			cfp->smcls.construct   = flags & Class::FlagNew       ? obj_new  : NULL;
		}

		// Build the object
		JSObject* obj = JS_NewObject(ctx, &cfp->smcls, NULL, NULL);
		if (!obj || !JS_SetPrivate(ctx, obj, cls ? cfp : NULL)) {
			delete cfp;
			return newUndefined().toException();
		}
		return Value(SpiderMonkeyEngineValue::getInstance(glb, OBJECT_TO_JSVAL(obj)));
	}

	virtual Value   newNull() {
		return Value(SpiderMonkeyEngineValue::getInstance(glb, JSVAL_NULL));
	}

	virtual Value   newUndefined() {
		return Value(SpiderMonkeyEngineValue::getInstance(glb, JSVAL_VOID));
	}

	virtual Value   evaluate(string jscript, string filename, unsigned int lineno=0, bool shift=false) {
		jsval val;
		if (JS_EvaluateScript(ctx, shift ? toJSObject() : JS_GetGlobalObject(ctx), jscript.c_str(), jscript.length(), filename.c_str(), lineno, &val) == JS_FALSE) {
			if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &val))
				return Value(SpiderMonkeyEngineValue::getInstance(glb, val)).toException();
			return newUndefined();
		}
		return Value(SpiderMonkeyEngineValue::getInstance(glb, val));
	}

	void getContext(void **context, void **value) {
		if (context) *context = &ctx;
		if (value) *value = &val;
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
		JSClass* jsc = NULL;
		jsval v;
		if (!JSVAL_IS_OBJECT(val)
				|| !(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->convert
				|| !jsc->convert(ctx, toJSObject(), JSTYPE_STRING, &v))
			JS_ConvertValue(ctx, val, JSTYPE_STRING, &v);

		return JS_EncodeString(ctx, JS_ValueToString(ctx, v));
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
		return Value(SpiderMonkeyEngineValue::getInstance(glb, v));
	}

	virtual Value   get(long idx) {
		jsval val = JSVAL_VOID;
		JSClass* jsc = NULL;
		if (!(jsc = JS_GetClass(ctx, toJSObject()))
				|| !jsc->getProperty
				||  jsc->getProperty == JS_PropertyStub
				|| !jsc->getProperty(ctx, toJSObject(), toJSID(idx), &val))
			JS_GetElement(ctx, toJSObject(), idx, &val);
		return Value(SpiderMonkeyEngineValue::getInstance(glb, val));
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

	virtual std::set<string> enumerate() {
		std::set<string> names;

		Value proto = get("prototype");
		if (!proto.isUndefined())
			names = proto.enumerate();

		JSIdArray* na = JS_Enumerate(ctx, toJSObject());
		if (!na) return names;

		for (jsint i=0 ; i < na->length ; i++) {
			jsval str;
			if (!JS_IdToValue(ctx, na->vector[i], &str)) break;
			names.insert(SpiderMonkeyEngineValue(glb, str).toString());
		}

		JS_DestroyIdArray(ctx, na);
		return names;
	}

	virtual PrivateMap* getPrivateMap() {
		CFP *cfp = getCFP(ctx, val);
		if (!cfp) return NULL;
		return &cfp->priv;
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

		Value v = Value(SpiderMonkeyEngineValue::getInstance(glb, rval));
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
		delete[] argv;

		Value v = Value(SpiderMonkeyEngineValue::getInstance(glb, rval));
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
	CFP *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss)
		return JS_FALSE;

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, key));
	Value res;

	if (vkey.isString())
		res = cfp->clss->del(obj, vkey.toString());
	else if (vkey.isNumber())
		res = cfp->clss->del(obj, vkey.toLong());
	else
		assert(false);

	if (res.isException()) {
		if (res.isUndefined()) {
			if (vkey.isString())
				JS_DeleteProperty(ctx, object, vkey.toString().c_str());
			else if (vkey.isNumber())
				JS_DeleteElement(ctx, object, vkey.toInt());
			else
				assert(false);
			return true;
		}
		JS_SetPendingException(ctx, getJSValue(res));
		return false;
	}
	return true;
}

static JSBool obj_get(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	CFP *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, key));
	Value res;
	if (vkey.isString())
		res = cfp->clss->get(obj, vkey.toString());
	else if (vkey.isNumber())
		res = cfp->clss->get(obj, vkey.toLong());
	else
		assert(false);

	if (res.isException()) {
		if (res.isUndefined())
			return true;
		JS_SetPendingException(ctx, getJSValue(res));
		return false;
	}
	*vp = getJSValue(res);
	return true;
}

static JSBool obj_set(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	CFP *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	jsval key;
	JS_IdToValue(ctx, id, &key);
	Value obj  = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, OBJECT_TO_JSVAL(object)));
	Value vkey = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, key));
	Value val  = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, *vp));
	Value res;
	if (vkey.isString())
		res = cfp->clss->set(obj, vkey.toString(), val);
	else if (vkey.isNumber())
		res = cfp->clss->set(obj, vkey.toLong(), val);
	else
		assert(false);

	if (res.isException()) {
		if (res.isUndefined())
			return true;
		JS_SetPendingException(ctx, getJSValue(res));
		return false;
	}
	return false;
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

	CFP *cfp = getCFP(ctx, OBJECT_TO_JSVAL(object));
	if (!cfp || !cfp->clss) {
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	// Call our callback
	Value obj = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, OBJECT_TO_JSVAL(object)));
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
	CFP *cfp = getCFP(ctx, JS_CALLEE(ctx, vp));
	if (!cfp || (!cfp->func && !cfp->clss)){
		JS_SetPendingException(ctx, STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, "Unable to find native info!")));
		return JS_FALSE;
	}

	// Allocate our array of arguments
	vector<Value> args = vector<Value>();
	for (unsigned int i=0; i < argc ; i++)
		args.push_back(Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, JS_ARGV(ctx, vp)[i])));

	// Make the call
	Value fnc = Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, JS_CALLEE(ctx, vp)));
	Value ths = constr ? fnc.newUndefined() : Value(SpiderMonkeyEngineValue::getInstance(cfp->glbl, JS_THIS(ctx, vp)));
	Value res = cfp->clss
					? (constr
						? cfp->clss->callNew(fnc, args)
						: cfp->clss->call(fnc, args))
					: cfp->func(ths, fnc, args);

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
