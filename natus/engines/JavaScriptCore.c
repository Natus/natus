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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/backend.h>

#ifdef __APPLE__
// JavaScriptCore.h requires CoreFoundation
// This is only found on Mac OS X
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

#define CTX(v) (((JavaScriptCoreValue*) v)->ctx)
#define VAL(v) (((JavaScriptCoreValue*) v)->val)
#define OBJ(v) (JSValueToObject(CTX(v), VAL(v), NULL))
#define ENG(v) ((JavaScriptCoreEngine*) v->eng->engine)

typedef struct {
	JSClassRef global;
	JSClassRef object;
	JSClassRef function;
} JavaScriptCoreEngine;

typedef struct {
	ntValue      hdr;
	JSContextRef ctx;
	JSValueRef   val;
} JavaScriptCoreValue;

typedef struct {
	JSClassDefinition def;
	JSClassRef        ref;
} JavaScriptCoreClass;

static ntValue* get_instance(const ntValue* ctx, JSValueRef val, bool exc)  {
	if (JSValueIsObject(CTX(ctx), val)) {
		ntValue* global = (ntValue*) nt_private_get(JSObjectGetPrivate(JSValueToObject(CTX(ctx), val, NULL)), NATUS_PRIV_GLOBAL);
		if (global && VAL(global) == val) return nt_value_incref(global);
	}

	ntValue *self = malloc(sizeof(JavaScriptCoreValue));
	if (!self) return self;
	memset(self, 0, sizeof(JavaScriptCoreValue));

	CTX(self) = CTX(ctx);

	if (!val) {
		VAL(self) = JSValueMakeUndefined(CTX(ctx));
		self->typ = ntValueTypeUndefined;
	} else {
		VAL(self) = val;
		self->typ = ntValueTypeUnknown;
	}
	if (!VAL(self)) {
		free(self);
		return NULL;
	}
	JSValueProtect(CTX(self), VAL(self));

	if (exc) self->typ |= ntValueTypeException;
	self->eng = nt_engine_incref(ctx->eng);
	self->ref = 1;
	return self;
}

static void class_free(JavaScriptCoreClass *jscc) {
	if (!jscc) return;
	if (jscc->ref)
		JSClassRelease(jscc->ref);
	free(jscc);
}

static size_t pows(size_t base, size_t pow) {
	if (pow == 0) return 1;
	return base * pows(base, pow-1);
}

static ntValue *property_to_value(ntValue *glbl, JSStringRef propertyName) {
	size_t i=0, idx=0, len = JSStringGetLength(propertyName);
	const JSChar *jschars = JSStringGetCharactersPtr(propertyName);
	if (!jschars) return NULL;

	for ( ; jschars && len-- > 0 ; i++) {
		if (jschars[len] < '0' || jschars[len] > '9')
			goto str;

		idx += (jschars[len] - '0') * pows(10, i);
	}
	if (i > 0)
		return nt_value_new_number(glbl, idx);

	str:
		return nt_value_new_string_utf16_length(glbl, jschars, JSStringGetLength(propertyName));
}

static inline void property_handler(JSContextRef ct, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exc, bool *retb, JSValueRef *retv) {
	// Bootstrap the basic private variables
	ntPrivate *prv = JSObjectGetPrivate(object);
	ntClass   *cls = nt_private_get(prv, NATUS_PRIV_CLASS);
	ntValue   *ctx = nt_private_get(prv, NATUS_PRIV_GLOBAL);
	assert(cls && ctx);

	// Convert the arguments
	ntValue *prp = property_to_value(ctx, propertyName);

	ntValue *obj = get_instance(ctx, object, false);
	ntValue *val = value ? get_instance(ctx, value, false) : NULL;
	if (!prp || !obj || (value && !val)) {
		nt_value_decref(val);
		nt_value_decref(obj);
		nt_value_decref(prp);
		*exc = NULL;
		if (retb) *retb = false;
		if (retv) *retv = NULL;
		return;
	}

	// Call the hook
	assert(prp->eng);
	assert(prp->eng->espec);
	assert(obj->eng);
	assert(obj->eng->espec);
	ntValue *rslt = NULL;
	if (retb) {
		if (value)
			rslt = cls->set(cls, obj, prp, val);
		else
			rslt = cls->del(cls, obj, prp);
	} else
		rslt = cls->get(cls, obj, prp);
	nt_value_decref(val);
	nt_value_decref(obj);
	nt_value_decref(prp);

	// Analyze the results
	if (nt_value_is_exception(rslt)) {
		*exc = !nt_value_is_undefined(rslt) ? VAL(rslt) : NULL;
		if (retv) *retv = NULL;
		if (retb) *retb = false;
		nt_value_decref(rslt);
		return;
	}
	if (retv) *retv = VAL(rslt);
	if (retb) *retb = true;
	nt_value_decref(rslt);
}

static void obj_finalize(JSObjectRef object) {
	nt_private_free(JSObjectGetPrivate(object));
}

static bool obj_del(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	bool ret;
	property_handler(ctx, object, propertyName, NULL, exc, &ret, NULL);
	return ret;
}

static JSValueRef obj_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	JSValueRef ret;
	property_handler(ctx, object, propertyName, NULL, exc, NULL, &ret);
	return ret;
}

static bool obj_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exc) {
	bool ret;
	property_handler(ctx, object, propertyName, value, exc, &ret, NULL);
	return ret;
}

static void obj_enum(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
	ntPrivate *prv = JSObjectGetPrivate(object);
	ntClass   *cls = nt_private_get(prv, NATUS_PRIV_CLASS);
	ntValue   *glb = nt_private_get(prv, NATUS_PRIV_GLOBAL);
	assert(cls && glb);

	ntValue *obj = get_instance(glb, object, false);
	if (!obj) return;

	ntValue *res = cls->enumerate(cls, obj);
	nt_value_decref(obj);
	if (!res) return;
	ntValue *len = nt_value_get_utf8(res, "length");

	long i, l = nt_value_to_long(len);
	nt_value_decref(len);
	for (i=0 ; i < l ; i++) {
		ntValue *item = nt_value_get_index(res, i);
		JSStringRef str = JSValueToStringCopy(ctx, VAL(item), NULL);
		nt_value_decref(item);
		if (!str) continue;
		JSPropertyNameAccumulatorAddName(propertyNames, str);
		JSStringRelease(str);
	}
	nt_value_decref(res);
}

static JSValueRef obj_call(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argc, const JSValueRef args[], JSValueRef* exc) {
	ntPrivate       *priv = JSObjectGetPrivate(object);
	ntValue         *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	ntNativeFunction func = nt_private_get(priv, NATUS_PRIV_FUNCTION);
	ntClass         *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
	assert(glbl && (func || clss));

	// Allocate our array of arguments
	JSObjectRef arga = JSObjectMakeArray(ctx, argc, args, exc);
	if (!arga) return NULL;

	// Do the call
	ntValue *fnc = get_instance(glbl, object, false);
	ntValue *ths = thisObject ? get_instance(glbl, thisObject, false) : nt_value_new_undefined(glbl);
	ntValue *arg = get_instance(glbl, arga, false);
	ntValue *res = func ? func(fnc, ths, arg) : clss->call(clss, fnc, ths, arg);
	nt_value_decref(arg);
	nt_value_decref(ths);
	nt_value_decref(fnc);

	JSValueRef r = res ? VAL(res) : NULL;
	if (nt_value_is_exception(res)) {
		nt_value_decref(res);
		*exc = r ? r : JSValueMakeUndefined(ctx);
		return NULL;
	}
	nt_value_decref(res);
	return r;
}

static JSObjectRef obj_new(JSContextRef ctx, JSObjectRef object, size_t argc, const JSValueRef args[], JSValueRef* exc) {
	return (JSObjectRef) obj_call(ctx, object, NULL, argc, args, exc);
}

static JSClassDefinition glbclassdef = {
	.className = "GlobalObject",
	.finalize  = obj_finalize,
};

static JSClassDefinition objclassdef = {
	.className = "NativeObject",
	.finalize  = obj_finalize,
};

static JSClassDefinition fncclassdef = {
	.className = "NativeFunction",
	.finalize  = obj_finalize,
	.callAsFunction = obj_call,
	.callAsConstructor = obj_new,
};

static ntPrivate       *jsc_value_private_get     (const ntValue *val) {
	return JSObjectGetPrivate(OBJ(val));
}

static bool             jsc_value_borrow_context  (const ntValue *ctx, void **context, void **value) {
	if (context) *context = &CTX(ctx);
	if (value)   *value   = &VAL(ctx);
	return true;
}

static ntValue*         jsc_value_get_global      (const ntValue *val) {
	return (ntValue*) nt_private_get(JSObjectGetPrivate(JSContextGetGlobalObject(CTX(val))), NATUS_PRIV_GLOBAL);
}

static ntValueType      jsc_value_get_type        (const ntValue *val) {
	JSType type = JSValueGetType(CTX(val), VAL(val));
	switch (type) {
		case kJSTypeBoolean:
			return ntValueTypeBoolean;
		case kJSTypeNull:
			return ntValueTypeNull;
		case kJSTypeNumber:
			return ntValueTypeNumber;
		case kJSTypeString:
			return ntValueTypeString;
		case kJSTypeObject:
			break;
		default:
			return ntValueTypeUndefined;
	}

	// FUNCTION
	if (JSObjectIsFunction(CTX(val), JSValueToObject(CTX(val), VAL(val), NULL))) {
		ntPrivate *priv = JSObjectGetPrivate(JSValueToObject(CTX(val), VAL(val), NULL));
		if (!priv || (nt_private_get(priv, NATUS_PRIV_FUNCTION) && !nt_private_get(priv, NATUS_PRIV_CLASS)))
			return ntValueTypeFunction;
	}

	// Bypass infinite loops for objects defined by natus
	// (otherwise, the JSObjectGetProperty() calls below will
	//  call the natus defined property handler, which may call
	//  get_type(), etc)
	if (JSObjectGetPrivate(OBJ(val))) return ntValueTypeObject;

	// ARRAY
	JSStringRef str = JSStringCreateWithUTF8CString("constructor");
	if (!str) return ntValueTypeUnknown;
	JSValueRef  prp = JSObjectGetProperty(CTX(val), OBJ(val), str, NULL);
	JSStringRelease(str);
	JSObjectRef obj = JSValueIsObject(CTX(val), prp) ? JSValueToObject(CTX(val), prp, NULL) : NULL;
	str = JSStringCreateWithUTF8CString("name");
	if (!str) return ntValueTypeUnknown;
	if (obj && (prp = JSObjectGetProperty(CTX(val), obj, str, NULL)) && JSValueIsString(CTX(val), prp)) {
		JSStringRelease(str);
		str = JSValueToStringCopy(CTX(val), prp, NULL);
		if (!str) return ntValueTypeUnknown;
		if (JSStringIsEqualToUTF8CString(str, "Array")) {
			JSStringRelease(str);
			return ntValueTypeArray;
		}
	}
	JSStringRelease(str);

	// OBJECT
	return ntValueTypeObject;
}

static void             jsc_value_free            (ntValue *val) {
	JSValueUnprotect(CTX(val), VAL(val));
	if (nt_value_is_global(val)) {
		JSGarbageCollect(CTX(val));
		JSGlobalContextRelease((JSGlobalContextRef) CTX(val));
	}
	nt_engine_decref(val->eng);
	free(val);
}

static ntValue*         jsc_value_new_bool        (const ntValue *ctx, bool b) {
	return get_instance(ctx, JSValueMakeBoolean(CTX(ctx), b), false);
}

static ntValue*         jsc_value_new_number      (const ntValue *ctx, double n) {
	return get_instance(ctx, JSValueMakeNumber(CTX(ctx), n), false);
}

static ntValue*         jsc_value_new_string_utf8(const ntValue *ctx, const char *string, size_t len) {
	JSStringRef str = JSStringCreateWithUTF8CString(string);
	JSValueRef  val = JSValueMakeString(CTX(ctx), str);
	JSStringRelease(str);
	return get_instance(ctx, val, false);
}

static ntValue*         jsc_value_new_string_utf16(const ntValue *ctx, const ntChar *string, size_t len) {
	JSStringRef str = JSStringCreateWithCharacters(string, len);
	JSValueRef  val = JSValueMakeString(CTX(ctx), str);
	JSStringRelease(str);
	return get_instance(ctx, val, false);
}

static ntValue*         jsc_value_new_array       (const ntValue *ctx, const ntValue **array) {
	int i;
	for (i=0 ; array && array[i] ; i++);

	JSValueRef *vals = calloc(i+1, sizeof(JSValueRef));
	memset(vals, 0, sizeof(JSValueRef) * (i+1));
	for (i=0 ; array && array[i] ; i++)
		vals[i] = VAL(array[i]);

	JSValueRef exc = NULL;
	JSObjectRef obj = JSObjectMakeArray(CTX(ctx), i, vals, &exc);
	free(vals);

	return get_instance(ctx, obj ? obj : exc, obj ? false : true);
}

static ntValue*         jsc_value_new_function    (const ntValue *ctx, ntPrivate *priv) {
	JSValueRef val = JSObjectMake(CTX(ctx), ENG(ctx)->function, priv);
	if (!val) return NULL;
	return get_instance(ctx, val, false);
}

static ntValue*         jsc_value_new_object      (const ntValue *ctx, ntClass *cls, ntPrivate *priv) {
	// Fill in our functions
	JSClassRef jscls = ENG(ctx)->object;
	if (cls) {
		JavaScriptCoreClass *jscc = malloc(sizeof(JavaScriptCoreClass));
		if (!jscc) return NULL;

		memset(jscc, 0, sizeof(JavaScriptCoreClass));
		jscc->def.finalize          = obj_finalize;
		jscc->def.className         = cls->call      ? "NativeFunction" : "NativeObject";
		jscc->def.getProperty       = cls->get       ? obj_get  : NULL;
		jscc->def.setProperty       = cls->set       ? obj_set  : NULL;
		jscc->def.deleteProperty    = cls->del       ? obj_del  : NULL;
		jscc->def.getPropertyNames  = cls->enumerate ? obj_enum : NULL;
		jscc->def.callAsFunction    = cls->call      ? obj_call : NULL;
		jscc->def.callAsConstructor = cls->call      ? obj_new  : NULL;

		if (!nt_private_set(priv, "natus.JavaScriptCore.JSClass", jscc, (ntFreeFunction) class_free))
			return NULL;

		jscc->ref = jscls = JSClassCreate(&jscc->def);
		if (!jscls) return NULL;
	}

	// Build the object
	JSObjectRef obj = JSObjectMake(CTX(ctx), jscls, priv);
	if (!obj) return NULL;

	// Do the return
	return get_instance(ctx, obj, false);
}

static ntValue*         jsc_value_new_null        (const ntValue *ctx) {
	return get_instance(ctx, JSValueMakeNull(CTX(ctx)), false);
}

static ntValue*         jsc_value_new_undefined   (const ntValue *ctx) {
	return get_instance(ctx, JSValueMakeUndefined(CTX(ctx)), false);
}

static bool             jsc_value_to_bool         (const ntValue *val) {
	return JSValueToBoolean(CTX(val), VAL(val));
}

static double           jsc_value_to_double       (const ntValue *val) {
	return JSValueToNumber(CTX(val), VAL(val), NULL);
}

static char            *jsc_value_to_string_utf8  (const ntValue *val, size_t *len) {
	JSStringRef str = JSValueToStringCopy(CTX(val), VAL(val), NULL);
	if (!str) return NULL;

	*len = JSStringGetMaximumUTF8CStringSize(str);
	char *buff = calloc(*len, sizeof(char));
	if (!buff) {
		JSStringRelease(str);
		return NULL;
	}

	*len = JSStringGetUTF8CString(str, buff, *len);
	if (*len > 0) *len -= 1;
	JSStringRelease(str);
	return buff;
}

static ntChar          *jsc_value_to_string_utf16 (const ntValue *val, size_t *len) {
	JSStringRef str = JSValueToStringCopy(CTX(val), VAL(val), NULL);
	if (!str) return NULL;

	*len = JSStringGetLength(str);
	ntChar *buff = calloc(*len+1, sizeof(ntChar));
	if (!buff) {
		JSStringRelease(str);
		return NULL;
	}
	memcpy(buff, JSStringGetCharactersPtr(str), *len * sizeof(JSChar));
	buff[*len] = '\0';
	JSStringRelease(str);

	return buff;
}

static ntValue         *jsc_value_del             (ntValue *obj, const ntValue *id) {
	JSValueRef exc = NULL;
	JSStringRef sidx = JSValueToStringCopy(CTX(obj), VAL(id), &exc);
	if (!sidx) return get_instance(obj, exc, true);

	JSObjectDeleteProperty(CTX(obj), OBJ(obj), sidx, &exc);
	JSStringRelease(sidx);
	if (exc) return get_instance(obj, exc, true);
	return get_instance(obj, JSValueMakeBoolean(CTX(obj), true), false);
}

static ntValue         *jsc_value_get    (const ntValue *obj, const ntValue *id) {
	JSValueRef exc = NULL;
	JSValueRef val = NULL;

	if (nt_value_is_number(id)) {
		val = JSObjectGetPropertyAtIndex(CTX(obj), OBJ(obj), nt_value_to_long(id), &exc);
	} else {
		JSStringRef sidx = JSValueToStringCopy(CTX(obj), VAL(id), NULL);
		if (!sidx) return NULL;
		val = JSObjectGetProperty(CTX(obj), OBJ(obj), sidx, &exc);
		JSStringRelease(sidx);
	}

	if (exc)  return get_instance(obj, exc, true);
	if (!val) return jsc_value_new_undefined(obj);
	ntValue *v = get_instance(obj, val, false);
	return v;
}

static ntValue         *jsc_value_set    (ntValue *obj, const ntValue *id, const ntValue *value, ntPropAttr attrs) {
	JSValueRef exc = NULL;
	if (nt_value_is_number(id)) {
		JSObjectSetPropertyAtIndex(CTX(obj), OBJ(obj), nt_value_to_long(id), VAL(value), &exc);
	} else {
		JSStringRef sidx = JSValueToStringCopy(CTX(obj), VAL(id), &exc);
		if (sidx) {
			JSPropertyAttributes flags = attrs == ntPropAttrNone ? ntPropAttrNone : attrs << 1;
			JSObjectSetProperty(CTX(obj), OBJ(obj), sidx, VAL(value), flags, &exc);
			JSStringRelease(sidx);
		}
	}
	return get_instance(obj, exc ? exc : JSValueMakeBoolean(CTX(obj), true), exc != NULL);
}

static ntValue         *jsc_value_enumerate       (const ntValue *obj) {
	size_t i;

	// Get the names
	JSPropertyNameArrayRef na = JSObjectCopyPropertyNames(CTX(obj), OBJ(obj));
	if (!na) return NULL;


	size_t c = JSPropertyNameArrayGetCount(na);
	JSValueRef *items = malloc(sizeof(JSValueRef *) * c);
	if (!items) {
		JSPropertyNameArrayRelease(na);
		return NULL;
	}

	for (i=0 ; i < c ; i++) {
		JSStringRef name = JSPropertyNameArrayGetNameAtIndex(na, i);
		if (name) {
			items[i] = JSValueMakeString(CTX(obj), name);
			if (items[i]) continue;
		}

		c = i;
		break;
	}
	JSPropertyNameArrayRelease(na);

	JSValueRef exc = NULL;
	JSObjectRef array = JSObjectMakeArray(CTX(obj), c, items, &exc);
	free(items);
	if (array)
		return get_instance(obj, array, false);
	return get_instance(obj, exc, true);
}

static ntValue         *jsc_value_call            (ntValue *func, ntValue *ths, ntValue *args) {
	// Convert to jsval array
	ntValue *length = nt_value_get_utf8(args, "length");
	long i, len = nt_value_to_long(length);
	nt_value_decref(length);

	JSValueRef *argv = calloc(len, sizeof(JSValueRef *));
	if (!argv) return NULL;
	for (i=0 ; i < len ; i++) {
		ntValue *item = nt_value_get_index(args, i);
		argv[i] = VAL(item);
		nt_value_decref(item);
	}

	// Call the function
	JSValueRef rval;
	JSValueRef exc = NULL;
	if (nt_value_get_type(ths) == ntValueTypeUndefined)
		rval = JSObjectCallAsConstructor(CTX(func), OBJ(func), i, argv, &exc);
	else
		rval = JSObjectCallAsFunction(CTX(func), OBJ(func), OBJ(ths), i, argv, &exc);
	free(argv);

	// Return the result
	return get_instance(func, exc == NULL ? rval : exc, exc != NULL);
}

static ntValue         *jsc_value_evaluate(ntValue *ths, const ntValue *jscript, const ntValue *filename, unsigned int lineno) {
	JSValueRef exc = NULL;

	JSStringRef strjscript  = JSValueToStringCopy(CTX(jscript), VAL(jscript), &exc);
	if (!strjscript) return get_instance(ths, exc, true);

	JSStringRef strfilename  = JSValueToStringCopy(CTX(filename), VAL(filename), &exc);
	if (!strfilename) {
		JSStringRelease(strjscript);
		return get_instance(ths, exc, true);
	}

	JSValueRef rval = JSEvaluateScript(CTX(ths), strjscript, OBJ(ths), strfilename, lineno, &exc);
	JSStringRelease(strjscript);
	JSStringRelease(strfilename);

	return get_instance(ths, exc == NULL ? rval : exc, exc != NULL);
}

static ntValue* jsc_engine_newg(void *engine, ntValue *global) {
	if (!engine) return NULL;

	JSContextGroupRef grp = NULL;
	if (global)
		grp = JSContextGetGroup(CTX(global));

	ntValue *self = malloc(sizeof(JavaScriptCoreValue));
	if (!self) return NULL;
	memset(self, 0, sizeof(JavaScriptCoreValue));

	CTX(self) = JSGlobalContextCreateInGroup(grp, ((JavaScriptCoreEngine *) engine)->global);
	if (!CTX(self)) {
		free(self);
		return NULL;
	}
	VAL(self) = JSContextGetGlobalObject(CTX(self));
	if (!VAL(self)) {
		free(self);
		JSGlobalContextRelease((JSGlobalContextRef) CTX(self));
		return NULL;
	}
	JSValueProtect(CTX(self), VAL(self));
	JSObjectSetPrivate(OBJ(self), nt_private_init());
	return self;
}

static void  jsc_engine_free(void *engine) {
	if (!engine) return;
	JSClassRelease(((JavaScriptCoreEngine *) engine)->global);
	JSClassRelease(((JavaScriptCoreEngine *) engine)->object);
	JSClassRelease(((JavaScriptCoreEngine *) engine)->function);
	free(engine);
}

static void* jsc_engine_init() {
	// Make sure the enum values don't get changed on us
	assert(kJSPropertyAttributeNone       == (JSPropertyAttributes) ntPropAttrNone);
	assert(kJSPropertyAttributeReadOnly   == (JSPropertyAttributes) ntPropAttrReadOnly   << 1);
	assert(kJSPropertyAttributeDontEnum   == (JSPropertyAttributes) ntPropAttrDontEnum   << 1);
	assert(kJSPropertyAttributeDontDelete == (JSPropertyAttributes) ntPropAttrDontDelete << 1);

	JavaScriptCoreEngine *eng = malloc(sizeof(JavaScriptCoreEngine));
	if (eng) {
		eng->global   = JSClassCreate(&glbclassdef);
		eng->object   = JSClassCreate(&objclassdef);
		eng->function = JSClassCreate(&fncclassdef);
		if (!eng->global || !eng->object || !eng->function) {
			JSClassRelease(eng->global);
			JSClassRelease(eng->object);
			JSClassRelease(eng->function);
			free(eng);
			return NULL;
		}
	}

	return eng;
}

NATUS_ENGINE("JavaScriptCore", "JSObjectMakeFunctionWithCallback", jsc);

