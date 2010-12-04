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
#include <cstdlib>
#include <cstdio>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/engine.h>
using namespace natus;

#ifdef __APPLE__
// JavaScriptCore.h requires CoreFoundation
// This is only found on Mac OS X
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

static JSClassDefinition fncdef;
static JSClassDefinition objdef;
static JSClassDefinition clldef;
static JSClassDefinition stkdef;
static JSClassRef fnccls;
static JSClassRef objcls;
static JSClassRef cllcls;
static JSClassRef stkcls;

static JSValueRef getJSValue(Value& value);
static string JSStringToString(JSStringRef str, bool release=false);

class JavaScriptCoreEngineValue : public EngineValue {
	friend JSValueRef getJSValue(Value& value);
public:
	JavaScriptCoreEngineValue(JSContextRef ctx) : EngineValue(this, false) {
		isArrayType = 0;
		this->ctx = ctx;
		this->val = JSContextGetGlobalObject(ctx);
		if (!this->val) throw bad_alloc();
		JSValueProtect(ctx, val);
	}

	JavaScriptCoreEngineValue(EngineValue* glb, JSValueRef val, bool exc=false) : EngineValue(glb, exc) {
		isArrayType = 0;
		this->ctx = static_cast<JavaScriptCoreEngineValue*>(glb)->ctx;
		this->val = val ? val : JSValueMakeUndefined(ctx);
		if (!this->val) throw bad_alloc();
		JSValueProtect(ctx, val);
	}

	virtual ~JavaScriptCoreEngineValue() {
		JSValueUnprotect(ctx, val);
		if (isGlobal()) {
			JSGarbageCollect(ctx);
			JSGlobalContextRelease((JSGlobalContextRef) ctx);
		}
	}

	virtual bool supportsPrivate() {
		return JSValueGetType(ctx, val) == kJSTypeObject && JSObjectGetPrivate(toJSObject());
	}

	virtual Value   newBool(bool b) {
		return Value(new JavaScriptCoreEngineValue(glb, JSValueMakeBoolean(ctx, b)));
	}

	virtual Value   newNumber(double n) {
		return Value(new JavaScriptCoreEngineValue(glb, JSValueMakeNumber(ctx, n)));
	}

	virtual Value   newString(string str) {
		return Value(new JavaScriptCoreEngineValue(glb, JSValueMakeString(ctx, JSStringCreateWithUTF8CString(str.c_str()))));
	}

	virtual Value   newArray(vector<Value> array) {
		JSValueRef *vals = new JSValueRef[array.size()];
		for (unsigned int i=0 ; i < array.size() ; i++)
			vals[i] = getJSValue(array[i]);
		JSObjectRef obj = JSObjectMakeArray(ctx, array.size(), vals, NULL);
		delete[] vals;
		return Value(new JavaScriptCoreEngineValue(glb, obj));
	}

	virtual Value   newFunction(NativeFunction func, void *priv, FreeFunction free) {
		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = NULL;
		cfp->func = func;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		JSValueRef val = (JSValueRef) JSObjectMake(ctx, fnccls, cfp);
		if (!val) delete cfp;
		return Value(new JavaScriptCoreEngineValue(glb, val));
	}

	virtual Value   newObject(Class* cls, void* priv, FreeFunction free) {
		ClassFuncPrivate *cfp = new ClassFuncPrivate();
		cfp->clss = cls;
		cfp->func = NULL;
		cfp->priv = priv;
		cfp->free = free;
		cfp->glbl = glb;

		// Pick what type of object to build
		JSClassRef wkcls = NULL;
		if (!cls)
			wkcls = stkcls; // Stock class, just a finalizer
		else if (cls->getFlags() & Class::Callable)
			wkcls = cllcls; // Full class, all methods
		else if (cls->getFlags() & Class::Object)
			wkcls = objcls; // Property methods only
		else
			assert(false);

		// Build the object
		JSObjectRef obj = JSObjectMake(ctx, wkcls, cfp);
		if (!obj) delete cfp;
		Value val = Value(new JavaScriptCoreEngineValue(glb, obj));
		return val;
	}

	virtual Value   newNull() {
		return Value(new JavaScriptCoreEngineValue(glb, JSValueMakeNull(ctx)));
	}

	virtual Value   newUndefined() {
		return Value(new JavaScriptCoreEngineValue(glb, JSValueMakeUndefined(ctx)));
	}

	virtual Value   evaluate(string jscript, string filename, unsigned int lineno=0, bool shift=false) {
		JSStringRef strjscript  = JSStringCreateWithUTF8CString(jscript.c_str());
		JSStringRef strfilename = JSStringCreateWithUTF8CString(filename.c_str());
		JSValueRef exc = NULL;
		JSValueRef rval = JSEvaluateScript(ctx, strjscript, shift ? toJSObject() : JSContextGetGlobalObject(ctx), strfilename, lineno, &exc);
		JSStringRelease(strjscript);
		JSStringRelease(strfilename);

		return Value(new JavaScriptCoreEngineValue(glb, exc == NULL ? rval : exc, exc != NULL));
	}

	virtual Value   getGlobal() {
		return Value(glb);
	}

	void getContext(void **context, void **value) {
		if (context) *context = &ctx;
		if (value) *value = &val;
	}

	virtual bool    isGlobal() {
		return JSContextGetGlobalObject(ctx) == val;
	}

	virtual bool    isArray() {
		if (isArrayType == 0) {
			isArrayType = -1;
			if (JSValueGetType(ctx, val) == kJSTypeObject) {
				Value array = glb->get("Array");
				JSObjectRef arrayobj = JSValueToObject(ctx, borrowInternal<JavaScriptCoreEngineValue>(array)->val, NULL);
				isArrayType = JSValueIsInstanceOfConstructor(ctx, val, arrayobj, NULL) ? 1 : -1;
			}
		}
		return isArrayType > 0;
	}

	virtual bool    isBool() {
		return JSValueGetType(ctx, val) == kJSTypeBoolean;
	}

	virtual bool    isFunction() {
		if (JSValueGetType(ctx, val) != kJSTypeObject) return false;
		JSObjectRef obj = JSValueToObject(ctx, val, NULL);
		if (!obj) return false; // Error
		if (!JSObjectIsFunction(ctx, obj)) return false;
		ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(obj);
		if (!cfp || (cfp->func && !cfp->clss))
			return true;
		return false;
	}

	virtual bool    isNull() {
		return JSValueGetType(ctx, val) == kJSTypeNull;
	}

	virtual bool    isNumber() {
		return JSValueGetType(ctx, val) == kJSTypeNumber;
	}

	virtual bool    isObject() {
		return !isUndefined() && JSValueGetType(ctx, val) == kJSTypeObject && !isArray() && !isFunction();
	}

	virtual bool    isString() {
		return JSValueGetType(ctx, val) == kJSTypeString;
	}

	virtual bool    isUndefined() {
		return JSValueGetType(ctx, val) == kJSTypeUndefined;
	}

	virtual bool    toBool() {
		return JSValueToBoolean(ctx, val);
	}

	virtual double  toDouble() {
		return JSValueToNumber(ctx, val, NULL);
	}

	virtual string  toString() {
		JSStringRef str = JSValueToStringCopy(ctx, val, NULL);
		if (!str) return "<unknown object>";

		size_t size = JSStringGetMaximumUTF8CStringSize(str) + 1;
		char  *buff = new char[size];

		JSStringGetUTF8CString(str, buff, size);
		JSStringRelease(str);

		string ret = buff;
		delete[] buff;
		return ret;
	}

	virtual bool    del(string name) {
		JSStringRef str = JSStringCreateWithUTF8CString(name.c_str());
		bool res = JSObjectDeleteProperty(ctx, toJSObject(), str, NULL);
		JSStringRelease(str);
		return res;
	}

	virtual bool    del(long idx) {
		JSValueRef  indx = JSValueMakeNumber(ctx, idx);
		JSStringRef sidx = JSValueToStringCopy(ctx, indx, NULL);
		bool res = JSObjectDeleteProperty(ctx, toJSObject(), sidx, NULL);
		JSStringRelease(sidx);
		return res;
	}

	virtual Value   get(string name) {
		JSStringRef str = JSStringCreateWithUTF8CString(name.c_str());
		JSValueRef  val = JSObjectGetProperty(ctx, toJSObject(), str, NULL);
		JSStringRelease(str);
		return Value(new JavaScriptCoreEngineValue(glb, val));
	}

	virtual Value   get(long idx) {
		JSValueRef val = JSObjectGetPropertyAtIndex(ctx, toJSObject(), idx, NULL);
		if (!val) return newUndefined();
		return Value(new JavaScriptCoreEngineValue(glb, val));
	}

	virtual bool    set(string name, Value value, Value::PropAttrs attrs) {
		// Make sure the enum values don't get changed on us
		assert(kJSPropertyAttributeNone       == (JSPropertyAttributes) None);
		assert(kJSPropertyAttributeReadOnly   == (JSPropertyAttributes) ReadOnly   << 1);
		assert(kJSPropertyAttributeDontEnum   == (JSPropertyAttributes) DontEnum   << 1);
		assert(kJSPropertyAttributeDontDelete == (JSPropertyAttributes) DontDelete << 1);

		JSValueRef exc = NULL;
		JSStringRef str = JSStringCreateWithUTF8CString(name.c_str());
		JSPropertyAttributes flags = attrs == None ? None : attrs << 1;
		JSObjectSetProperty(ctx, toJSObject(), str, getJSValue(value), flags, &exc);
		JSStringRelease(str);
		return exc == NULL;
	}

	virtual bool    set(long idx, Value value) {
		JSValueRef exc = NULL;
		JSObjectSetPropertyAtIndex(ctx, toJSObject(), idx, getJSValue(value), &exc);
		return exc == NULL;
	}

	virtual std::set<string>  enumerate() {
		std::set<string> names;

		JSPropertyNameArrayRef na = JSObjectCopyPropertyNames(ctx, toJSObject());
		if (!na) return names;

		size_t c = JSPropertyNameArrayGetCount(na);
		for (size_t i=0 ; i < c ; i++) {
			JSStringRef name = JSPropertyNameArrayGetNameAtIndex(na, i);
			if (!name) break;
			names.insert(JSStringToString(name, true));
		}
		JSPropertyNameArrayRelease(na);
		return names;
	}

	virtual bool    setPrivate(void *priv) {
		ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(toJSObject());
		if (!cfp) return false;
		cfp->priv = priv;
		return true;
	}

	virtual void*   getPrivate() {
		ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(toJSObject());
		if (!cfp) return NULL;
		return cfp->priv;
	}

	virtual Value   call(Value func, vector<Value> args) {
		// Convert to jsval array
		JSValueRef *argv = new JSValueRef[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[i]);

		// Call the function
		JSValueRef exc = NULL;
		JSValueRef rval;
		rval = JSObjectCallAsFunction(ctx,
				borrowInternal<JavaScriptCoreEngineValue>(func)->toJSObject(),
				toJSObject(), args.size(), argv, &exc);
		delete[] argv;

		return Value(new JavaScriptCoreEngineValue(glb, exc == NULL ? rval : exc, exc != NULL));
	}

	virtual Value   callNew(vector<Value> args) {
		// Convert to jsval array
		JSValueRef *argv = new JSValueRef[args.size()];
		for (unsigned int i=0 ; i < args.size() ; i++)
			argv[i] = getJSValue(args[i]);

		// Call the function
		JSValueRef exc = NULL;
		JSValueRef rval;
		rval = JSObjectCallAsConstructor(ctx, toJSObject(), args.size(), argv, &exc);
		delete[] argv;

		return Value(new JavaScriptCoreEngineValue(glb, exc == NULL ? rval : exc, exc != NULL));
	}

private:
	JSContextRef       ctx;
	JSValueRef         val;
	int                isArrayType;

	JSObjectRef toJSObject() {
		return JSValueToObject(ctx, val, NULL);
	}
};

static JSValueRef getJSValue(Value& value) {
	return JavaScriptCoreEngineValue::borrowInternal<JavaScriptCoreEngineValue>(value)->val;
}

static string JSStringToString(JSStringRef str, bool release) {
	size_t size = JSStringGetMaximumUTF8CStringSize(str);
	char *buffer = new char[size];
	JSStringGetUTF8CString(str, buffer, size);
	string ret(buffer);
	delete[] buffer;
	if (release) JSStringRelease(str);
	return ret;
}

static void finalize(JSObjectRef object) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(object);
	delete cfp;
}

static JSValueRef fnc_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exc) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(function);
	if (!cfp || !cfp->func) {
		*exc = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Unable to find native function info!"));
		return NULL;
	}
	if (!cfp->func) return NULL;

	// Allocate our array of arguments
	vector<Value> args = vector<Value>();
	for (unsigned int i=0; i < argumentCount ; i++)
		args.push_back(Value(new JavaScriptCoreEngineValue(cfp->glbl, arguments[i])));

	Value fnc = Value(new JavaScriptCoreEngineValue(cfp->glbl, function));
	Value ths = thisObject ? Value(new JavaScriptCoreEngineValue(cfp->glbl, thisObject)) : fnc.newUndefined();
	Value res = cfp->func(ths, fnc, args, cfp->priv);
	if (res.isException()) {
		*exc = getJSValue(res);
		return NULL;
	}
	return getJSValue(res);
}

static JSObjectRef fnc_new(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	return (JSObjectRef) fnc_call(ctx, constructor, NULL, argumentCount, arguments, exception);
}

static bool obj_del(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(object);
	if (!cfp || !cfp->clss) {
		*exc = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Unable to find native function info!"));
		return false;
	}

	Value obj = Value(new JavaScriptCoreEngineValue(cfp->glbl, object));

	// Find out if we are a numeric or string property
	char *skey = new char[JSStringGetMaximumUTF8CStringSize(propertyName) + 1];
	JSStringGetUTF8CString(propertyName, skey, JSStringGetMaximumUTF8CStringSize(propertyName) + 1);
	char *end = NULL;
	long idx = strtol(skey, &end, 0);

	// We have numeric
	if (end && *end == '\0') {
		delete[] skey;
		return cfp->clss->del(obj, idx);
	}

	// We have string
	delete[] skey;
	return cfp->clss->del(obj, JSStringToString(propertyName));
}

static JSValueRef obj_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(object);
	if (!cfp || !cfp->clss) {
		*exc = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Unable to find native function info!"));
		return NULL;
	}

	Value obj = Value(new JavaScriptCoreEngineValue(cfp->glbl, object));

	// Find out if we are a numeric or string property
	char *skey = new char[JSStringGetMaximumUTF8CStringSize(propertyName) + 1];
	JSStringGetUTF8CString(propertyName, skey, JSStringGetMaximumUTF8CStringSize(propertyName) + 1);
	char *end = NULL;
	long int idx = strtol(skey, &end, 0);

	// We have numeric
	if (end && *end == '\0') {
		delete[] skey;
		Value res = cfp->clss->get(obj, idx);
		if (res.isException()) {
			*exc = getJSValue(res);
			return NULL;
		}
		return getJSValue(res);
	}

	// We have string
	delete[] skey;
	Value res = cfp->clss->get(obj, JSStringToString(propertyName));
	if (res.isException()) {
		*exc = getJSValue(res);
		return NULL;
	}
	return getJSValue(res);
}

static bool obj_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exc) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(object);
	if (!cfp || !cfp->clss) {
		*exc = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Unable to find native function info!"));
		return NULL;
	}

	Value obj = Value(new JavaScriptCoreEngineValue(cfp->glbl, object));
	Value val = Value(new JavaScriptCoreEngineValue(cfp->glbl, value));

	// Find out if we are a numeric or string property
	char *skey = new char[JSStringGetMaximumUTF8CStringSize(propertyName) + 1];
	JSStringGetUTF8CString(propertyName, skey, JSStringGetMaximumUTF8CStringSize(propertyName) + 1);
	char *end = NULL;
	long int idx = strtol(skey, &end, 0);

	// We have numeric
	if (end && *end == '\0') {
		delete skey;
		return cfp->clss->set(obj, idx, val);
	}

	// We have string
	delete skey;
	return cfp->clss->set(obj, JSStringToString(propertyName), val);
}

static void obj_enum(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(object);
	if (!cfp || !cfp->clss) return;

	Value obj = Value(new JavaScriptCoreEngineValue(cfp->glbl, object));
	Value res = cfp->clss->enumerate(obj);
	long len = res.length();
	for (long i=0 ; i < len ; i++) {
		Value item = res.get(i);
		if (!item.isString()) continue;
		JSStringRef str = JSValueToStringCopy(ctx, getJSValue(item), NULL);
		if (!str) continue;
		JSPropertyNameAccumulatorAddName(propertyNames, str);
		JSStringRelease(str);
	}
}

static JSValueRef obj_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exc) {
	ClassFuncPrivate *cfp = (ClassFuncPrivate *) JSObjectGetPrivate(function);
	if (!cfp || !cfp->clss) {
		*exc = JSValueMakeString(ctx, JSStringCreateWithUTF8CString("Unable to find native function info!"));
		return NULL;
	}

	// Prepare the arguments
	vector<Value> args = vector<Value>();
	for (size_t i=0 ; i < argumentCount ; i++)
		args.push_back(Value(new JavaScriptCoreEngineValue(cfp->glbl, arguments[i])));

	// Call the function
	Value obj = Value(new JavaScriptCoreEngineValue(cfp->glbl, function));
	Value res = thisObject ? cfp->clss->call(obj, args) : cfp->clss->callNew(obj, args);
	if (res.isException()) {
		*exc = getJSValue(res);
		return NULL;
	}
	return getJSValue(res);
}

static JSObjectRef obj_new(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	return (JSObjectRef) obj_call(ctx, constructor, NULL, argumentCount, arguments, exception);
}

static EngineValue* engine_newg(void *engine) {
	return new JavaScriptCoreEngineValue(JSGlobalContextCreate(NULL));
}

static void engine_free(void *engine) {
	JSClassRelease(stkcls);
	JSClassRelease(cllcls);
	JSClassRelease(objcls);
	JSClassRelease(fnccls);

	delete (char *) engine;
}

static void *engine_init() {
	fncdef.version = 0;
	fncdef.finalize = finalize;
	fncdef.callAsFunction = fnc_call;
	fncdef.callAsConstructor = fnc_new;

	objdef.version = 0;
	objdef.finalize = finalize;
	objdef.getProperty = obj_get;
	objdef.setProperty = obj_set;
	objdef.deleteProperty = obj_del;
	objdef.getPropertyNames = obj_enum;

	clldef.version = 0;
	clldef.finalize = finalize;
	clldef.getProperty = obj_get;
	clldef.setProperty = obj_set;
	clldef.deleteProperty = obj_del;
	clldef.getPropertyNames = obj_enum;
	clldef.callAsFunction = obj_call;
	clldef.callAsConstructor = obj_new;

	stkdef.version = 0;
	stkdef.finalize = finalize;

    fnccls = JSClassCreate(&fncdef);
    objcls = JSClassCreate(&objdef);
    cllcls = JSClassCreate(&clldef);
    stkcls = JSClassCreate(&stkdef);

	return new char;
}

NATUS_ENGINE("JavaScriptCore", "JSObjectMakeFunctionWithCallback", engine_init, engine_newg, engine_free);
