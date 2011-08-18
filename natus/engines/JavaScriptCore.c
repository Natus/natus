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
#include <stdbool.h>

#ifdef __APPLE__
// JavaScriptCore.h requires CoreFoundation
// This is only found on Mac OS X
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
typedef JSContextRef ntEngCtx;
typedef JSValueRef ntEngVal;
#include <natus/backend.h>

#define NATUS_PRIV_JSC_CLASS "natus.JavaScriptCore.JSClass"

#define checkerror() \
	if (exc) { \
		*flags |= ntEngValFlagException; \
		return exc; \
	}

#define checkerrorval(val) \
	checkerror(); \
	if (!val) \
		return NULL


typedef struct {
	JSClassDefinition def;
	JSClassRef ref;
} JavaScriptCoreClass;

static void class_free (JavaScriptCoreClass *jscc) {
	if (!jscc)
		return;
	if (jscc->ref)
		JSClassRelease (jscc->ref);
	free (jscc);
}

static size_t pows (size_t base, size_t pow) {
	if (pow == 0)
		return 1;
	return base * pows (base, pow - 1);
}

static ntEngVal property_to_value (ntEngCtx ctx, JSStringRef propertyName) {
	size_t i = 0, idx = 0, len = JSStringGetLength (propertyName);
	const JSChar *jschars = JSStringGetCharactersPtr (propertyName);
	if (!jschars)
		return NULL;

	for (; jschars && len-- > 0 ; i++) {
		if (jschars[len] < '0' || jschars[len] > '9')
			goto str;

		idx += (jschars[len] - '0') * pows (10, i);
	}
	if (i > 0)
		return JSValueMakeNumber(ctx, idx);

	str:
		return JSValueMakeString(ctx, propertyName);
}

static void obj_finalize (JSObjectRef object) {
	nt_private_free (JSObjectGetPrivate (object));
}

static bool obj_del (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	ntEngVal idx = property_to_value(ctx, propertyName);
	if (idx) {
		ntEngValFlags flags = ntEngValFlagNone;
		JSValueProtect(ctx, object);
		JSValueProtect(ctx, idx);
		JSValueRef ret = nt_value_handle_property(ntPropertyActionDelete, object, JSObjectGetPrivate(object), idx, NULL, &flags);
		if (flags & ntEngValFlagMustFree)
			JSValueUnprotect(ctx, ret);
		if (flags & ntEngValFlagException) {
			*exc = ret;
			return false;
		}

		return JSValueIsBoolean(ctx, ret) ? JSValueToBoolean(ctx, ret) : false;
	}
	return false;
}

static JSValueRef obj_get (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc) {
	ntEngVal idx = property_to_value(ctx, propertyName);
	if (idx) {
		ntEngValFlags flags = ntEngValFlagNone;
		JSValueProtect(ctx, object);
		JSValueProtect(ctx, idx);
		JSValueRef ret = nt_value_handle_property(ntPropertyActionGet, object, JSObjectGetPrivate(object), idx, NULL, &flags);
		if (flags & ntEngValFlagMustFree)
			JSValueUnprotect(ctx, ret);
		if (flags & ntEngValFlagException) {
			*exc = ret;
			return NULL;
		}

		return ret;
	}
	return NULL;
}

static bool obj_set (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exc) {
	ntEngVal idx = property_to_value(ctx, propertyName);
	if (idx) {
		ntEngValFlags flags = ntEngValFlagNone;
		JSValueProtect(ctx, object);
		JSValueProtect(ctx, idx);
		JSValueProtect(ctx, value);
		JSValueRef ret = nt_value_handle_property(ntPropertyActionSet, object, JSObjectGetPrivate(object), idx, value, &flags);
		if (flags & ntEngValFlagMustFree)
			JSValueUnprotect(ctx, ret);
		if (flags & ntEngValFlagException) {
			*exc = ret;
			return false;
		}

		return JSValueIsBoolean(ctx, ret) ? JSValueToBoolean(ctx, ret) : false;
	}
	return false;
}

static void obj_enum (JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
	JSValueRef exc = NULL;

	ntEngValFlags flags = ntEngValFlagNone;
	JSValueRef rslt = nt_value_handle_property(ntPropertyActionEnumerate, JSObjectGetPrivate(object), NULL, NULL, NULL, &flags);
	if (!rslt)
		return;
	if (flags & ntEngValFlagMustFree)
		JSValueUnprotect(ctx, rslt);

	JSObjectRef obj = JSValueToObject(ctx, rslt, &exc);
	if (!obj || exc)
		return;

	JSStringRef length = JSStringCreateWithUTF8CString("length");
	if (!length)
		return;
	JSValueRef len = JSObjectGetProperty(ctx, obj, length, &exc);
	JSStringRelease(length);
	if (!len || exc)
		return;

	int count = (int) JSValueToNumber(ctx, len, &exc);
	if (exc)
		return;

	int i;
	for (i = 0; i < count ; i++) {
		JSValueRef val = JSObjectGetPropertyAtIndex(ctx, obj, i, &exc);
		if (!val || exc)
			return;

		JSStringRef str = JSValueToStringCopy(ctx, val, NULL);
		if (!str)
			return;

		JSPropertyNameAccumulatorAddName(propertyNames, str);
		JSStringRelease(str);
	}
}

static JSValueRef obj_call (JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argc, const JSValueRef args[], JSValueRef* exc) {
	ntEngValFlags flags = ntEngValFlagNone;
	JSValueRef arg = JSObjectMakeArray(ctx, argc, args, exc);
	JSValueProtect(ctx, object);
	if (thisObject) JSValueProtect(ctx, thisObject);
	JSValueProtect(ctx, arg);
	JSValueRef ret = nt_value_handle_call(object, JSObjectGetPrivate(object), thisObject, arg, &flags);
	if (!ret) {
		*exc = JSValueMakeUndefined(ctx);
		return NULL;
	}

	if (flags & ntEngValFlagMustFree)
		JSValueUnprotect(ctx, ret);

	if (flags & ntEngValFlagException) {
		*exc = ret;
		return NULL;
	}

	return ret;
}

static JSObjectRef obj_new (JSContextRef ctx, JSObjectRef object, size_t argc, const JSValueRef args[], JSValueRef* exc) {
	return (JSObjectRef) obj_call (ctx, object, NULL, argc, args, exc);
}

static JSClassDefinition glbclassdef = { .className = "GlobalObject", .finalize = obj_finalize, };
static JSClassDefinition objclassdef = { .className = "NativeObject", .finalize = obj_finalize, };
static JSClassDefinition fncclassdef = { .className = "NativeFunction", .finalize = obj_finalize, .callAsFunction = obj_call, .callAsConstructor = obj_new, };
static JSClassRef glbcls;
static JSClassRef objcls;
static JSClassRef fnccls;

__attribute__((constructor))
static void _init() {
	// Make sure the enum values don't get changed on us
	assert(kJSPropertyAttributeNone == (JSPropertyAttributes) ntPropAttrNone);
	assert(kJSPropertyAttributeReadOnly == (JSPropertyAttributes) ntPropAttrReadOnly << 1);
	assert(kJSPropertyAttributeDontEnum == (JSPropertyAttributes) ntPropAttrDontEnum << 1);
	assert(kJSPropertyAttributeDontDelete == (JSPropertyAttributes) ntPropAttrDontDelete << 1);

	if (!glbcls)
		assert(glbcls = JSClassCreate(&glbclassdef));
	if (!objcls)
		assert(objcls = JSClassCreate(&objclassdef));
	if (!fnccls)
		assert(fnccls = JSClassCreate(&fncclassdef));
}

__attribute__((destructor))
static void _fini() {
	if (glbcls) {
		JSClassRelease(glbcls);
		glbcls = NULL;
	}
	if (objcls) {
		JSClassRelease(objcls);
		objcls = NULL;
	}
	if (fnccls) {
		JSClassRelease(fnccls);
		fnccls = NULL;
	}
}

static void jsc_ctx_free(ntEngCtx ctx) {
	JSGarbageCollect(ctx);
	JSGlobalContextRelease ((JSGlobalContextRef) ctx);
}

static void jsc_val_unlock(ntEngCtx ctx, ntEngVal val) {
	if (val != JSContextGetGlobalObject(ctx))
		JSValueUnprotect(ctx, val);
}

static ntEngVal jsc_val_duplicate(ntEngCtx ctx, ntEngVal val) {
	JSValueProtect(ctx, val);
	return val;
}

static void jsc_val_free(ntEngVal val) {

}

static ntEngVal jsc_new_global (ntEngCtx ctx, ntEngVal val, ntPrivate *priv, ntEngCtx *newctx, ntEngValFlags *flags) {
	*newctx = NULL;
	*flags = ntEngValFlagNone;

	JSContextGroupRef grp = NULL;
	if (ctx)
		grp = JSContextGetGroup (ctx);

	*newctx = JSGlobalContextCreateInGroup (grp, glbcls);
	if (!*newctx)
		goto error;

	JSObjectRef glb = JSContextGetGlobalObject (*newctx);
	if (!glb)
		goto error;

	if (!JSObjectSetPrivate (glb, priv))
		goto error;

	return glb;

	error:
		if (*newctx) {
			JSGlobalContextRelease ((JSGlobalContextRef) *newctx);
			*newctx = NULL;
		}
		nt_private_free(priv);
		return NULL;
}

static ntEngVal jsc_new_bool (const ntEngCtx ctx, bool b, ntEngValFlags *flags) {
	return JSValueMakeBoolean(ctx, b);
}

static ntEngVal jsc_new_number(const ntEngCtx ctx, double n, ntEngValFlags *flags) {
	return JSValueMakeNumber(ctx, n);
}

static ntEngVal jsc_new_string_utf8(const ntEngCtx ctx, const char *str, size_t len, ntEngValFlags *flags) {
	JSStringRef string = JSStringCreateWithUTF8CString (str);
	JSValueRef val = JSValueMakeString (ctx, string);
	JSStringRelease (string);
	return val;
}

static ntEngVal jsc_new_string_utf16(const ntEngCtx ctx, const ntChar *str, size_t len, ntEngValFlags *flags) {
	JSStringRef string = JSStringCreateWithCharacters (str, len);
	JSValueRef val = JSValueMakeString (ctx, string);
	JSStringRelease (string);
	return val;
}

static ntEngVal jsc_new_array(const ntEngCtx ctx, const ntEngVal *array, size_t len, ntEngValFlags *flags) {
	JSValueRef exc = NULL;
	JSObjectRef ret = JSObjectMakeArray (ctx, len, array, &exc);
	checkerrorval(ret);
	return ret;
}


static ntEngVal jsc_new_function(const ntEngCtx ctx, const char *name, ntPrivate *priv, ntEngValFlags *flags) {
	JSStringRef stmp;
	JSValueRef exc = NULL;

	// Get the function object and the prototype
	JSObjectRef global = JSContextGetGlobalObject(ctx);
	checkerrorval(global);

	stmp = JSStringCreateWithUTF8CString("Function");
	checkerrorval(stmp);
	JSValueRef function = JSObjectGetProperty(ctx, global, stmp, &exc);
	JSStringRelease(stmp);
	checkerrorval(function);

	JSObjectRef funcobj = JSValueToObject(ctx, function, &exc);
	checkerrorval(funcobj);
	stmp = JSStringCreateWithUTF8CString("prototype");
	checkerrorval(stmp);
	JSValueRef prototype = JSObjectGetProperty(ctx, funcobj, stmp, &exc);
	JSStringRelease(stmp);
	checkerrorval(prototype);

	// Make our new function (which is really an object we will convert to emulate a function)
	JSObjectRef obj = JSObjectMake (ctx, fnccls, priv);
	checkerrorval(obj);

	// Convert the object to emulate a function
	JSObjectSetPrototype(ctx, obj, prototype);

	// Add a proper toString() function
	JSStringRef fname = JSStringCreateWithUTF8CString("toString");
	checkerrorval(fname);
	stmp = JSStringCreateWithUTF8CString("return 'function ' + this.name + '() {\\n    [native code]\\n}';");
	if (!stmp) {
		JSStringRelease(fname);
		return NULL;
	}
	JSObjectRef toString = JSObjectMakeFunction(ctx, fname, 0, NULL, stmp, NULL, 0, &exc);
	JSStringRelease(stmp);
	if (exc) {
		JSStringRelease(fname);
		*flags |= ntEngValFlagException;
		return exc;
	}
	JSObjectSetProperty(ctx, obj, fname, toString, kJSPropertyAttributeNone, &exc);
	JSStringRelease(fname);
	checkerror();

	// Set the name
	fname = JSStringCreateWithUTF8CString(name ? name : "anonymous");
	checkerrorval(fname);
	stmp  = JSStringCreateWithUTF8CString("name");
	if (!stmp) {
		JSStringRelease(fname);
		return NULL;
	}
	JSObjectSetProperty(ctx, obj, stmp, JSValueMakeString(ctx, fname), kJSPropertyAttributeNone, &exc);
	JSStringRelease(stmp);
	JSStringRelease(fname);
	checkerror();

	stmp = JSStringCreateWithUTF8CString("constructor");
	checkerrorval(stmp);
	JSObjectSetProperty(ctx, obj, stmp, function, kJSPropertyAttributeNone, &exc);
	JSStringRelease(stmp);
	checkerror();

	return obj;
}

static ntEngVal jsc_new_object(const ntEngCtx ctx, ntClass *cls, ntPrivate *priv, ntEngValFlags *flags) {
	// Fill in our functions
	JSClassRef jscls = objcls;
	if (cls) {
		JavaScriptCoreClass *jscc = malloc (sizeof(JavaScriptCoreClass));
		if (!jscc)
			return NULL;

		memset (jscc, 0, sizeof(JavaScriptCoreClass));
		jscc->def.finalize = obj_finalize;
		jscc->def.className = cls->call ? "NativeFunction" : "NativeObject";
		jscc->def.getProperty = cls->get ? obj_get : NULL;
		jscc->def.setProperty = cls->set ? obj_set : NULL;
		jscc->def.deleteProperty = cls->del ? obj_del : NULL;
		jscc->def.getPropertyNames = cls->enumerate ? obj_enum : NULL;
		jscc->def.callAsFunction = cls->call ? obj_call : NULL;
		jscc->def.callAsConstructor = cls->call ? obj_new : NULL;

		if (!nt_private_set (priv, NATUS_PRIV_JSC_CLASS, jscc, (ntFreeFunction) class_free))
			return NULL;

		jscc->ref = jscls = JSClassCreate (&jscc->def);
		if (!jscls)
			return NULL;
	}

	// Build the object
	JSObjectRef obj = JSObjectMake (ctx, jscls, priv);
	if (!obj)
		return NULL;

	// Do the return
	return obj;
}

static ntEngVal jsc_new_null(const ntEngCtx ctx, ntEngValFlags *flags) {
	return JSValueMakeNull(ctx);
}

static ntEngVal jsc_new_undefined(const ntEngCtx ctx, ntEngValFlags *flags) {
	return JSValueMakeUndefined(ctx);
}

static bool jsc_to_bool(const ntEngCtx ctx, const ntEngVal val) {
	return JSValueToBoolean(ctx, val);
}

static double jsc_to_double(const ntEngCtx ctx, const ntEngVal val) {
	return JSValueToNumber(ctx, val, NULL);
}

static char *jsc_to_string_utf8(const ntEngCtx ctx, const ntEngVal val, size_t *len) {
	JSStringRef str = JSValueToStringCopy (ctx, val, NULL);
	if (!str)
		return NULL;

	*len = JSStringGetMaximumUTF8CStringSize (str);
	char *buff = calloc (*len, sizeof(char));
	if (!buff) {
		JSStringRelease (str);
		return NULL;
	}

	*len = JSStringGetUTF8CString (str, buff, *len);
	if (*len > 0)
		*len -= 1;
	JSStringRelease (str);
	return buff;
}

static ntChar *jsc_to_string_utf16(const ntEngCtx ctx, const ntEngVal val, size_t *len) {
	JSStringRef str = JSValueToStringCopy (ctx, val, NULL);
	if (!str)
		return NULL;

	*len = JSStringGetLength (str);
	ntChar *buff = calloc (*len + 1, sizeof(ntChar));
	if (!buff) {
		JSStringRelease (str);
		return NULL;
	}
	memcpy (buff, JSStringGetCharactersPtr (str), *len * sizeof(JSChar));
	buff[*len] = '\0';
	JSStringRelease (str);

	return buff;
}

static ntEngVal jsc_del(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags) {
	JSValueRef exc = NULL;

	JSObjectRef obj = JSValueToObject(ctx, val, &exc);
	checkerrorval(obj);

	JSStringRef sidx = JSValueToStringCopy (ctx, id, &exc);
	checkerrorval(sidx);

	JSObjectDeleteProperty (ctx, obj, sidx, &exc);
	JSStringRelease (sidx);
	checkerror();

	return JSValueMakeBoolean (ctx, true);
}

static ntEngVal jsc_get(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags) {
	JSValueRef exc = NULL;
	JSValueRef rslt = NULL;

	JSObjectRef obj = JSValueToObject(ctx, val, &exc);
	checkerrorval(obj);

	if (JSValueIsNumber(ctx, id)) {
		double idx = JSValueToNumber(ctx, id, &exc);
		checkerror();
		rslt = JSObjectGetPropertyAtIndex (ctx, obj, idx, &exc);
	} else {
		JSStringRef idx = JSValueToStringCopy (ctx, id, &exc);
		checkerrorval(idx);
		rslt = JSObjectGetProperty (ctx, obj, idx, &exc);
		JSStringRelease (idx);
	}
	checkerrorval(rslt);
	return rslt;
}

static ntEngVal jsc_set(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, const ntEngVal value, ntPropAttr attrs, ntEngValFlags *flags) {
	JSValueRef exc = NULL;

	JSObjectRef obj = JSValueToObject(ctx, val, &exc);
	checkerrorval(obj);

	if (JSValueIsNumber(ctx, id)) {
		double idx = JSValueToNumber(ctx, id, &exc);
		if (!exc)
			JSObjectSetPropertyAtIndex(ctx, obj, idx, value, &exc);
	} else {
		JSStringRef idx = JSValueToStringCopy (ctx, id, &exc);
		if (idx) {
			JSPropertyAttributes flags = attrs == ntPropAttrNone ? ntPropAttrNone : attrs << 1;
			JSObjectSetProperty (ctx, obj, idx, value, flags, &exc);
			JSStringRelease (idx);
		}
	}
	checkerror();
	return JSValueMakeBoolean (ctx, true);
}

static ntEngVal jsc_enumerate(const ntEngCtx ctx, ntEngVal val, ntEngValFlags *flags) {
	size_t i;
	JSValueRef exc = NULL;

	JSObjectRef obj = JSValueToObject(ctx, val, &exc);
	checkerrorval(obj);

	// Get the names
	JSPropertyNameArrayRef na = JSObjectCopyPropertyNames (ctx, obj);
	checkerrorval(na);

	size_t c = JSPropertyNameArrayGetCount (na);
	JSValueRef *items = malloc (sizeof(JSValueRef *) * c);
	if (!items) {
		JSPropertyNameArrayRelease (na);
		return NULL;
	}

	for (i = 0; i < c ; i++) {
		JSStringRef name = JSPropertyNameArrayGetNameAtIndex (na, i);
		if (name) {
			items[i] = JSValueMakeString (ctx, name);
			if (items[i])
				continue;
		}

		c = i;
		break;
	}
	JSPropertyNameArrayRelease (na);

	JSObjectRef array = JSObjectMakeArray (ctx, c, items, &exc);
	free (items);
	checkerror();
	return array;
}

static ntEngVal jsc_call(const ntEngCtx ctx, ntEngVal func, ntEngVal ths, ntEngVal args, ntEngValFlags *flags) {
	JSValueRef exc = NULL;

	JSObjectRef funcobj = JSValueToObject(ctx, func, &exc);
	checkerrorval(funcobj);

	JSObjectRef argsobj = JSValueToObject(ctx, args, &exc);
	checkerrorval(argsobj);

	// Convert to jsval array
	JSStringRef name = JSStringCreateWithUTF8CString("length");
	checkerrorval(name);
	JSValueRef length = JSObjectGetProperty(ctx, argsobj, name, &exc);
	checkerrorval(length);
	long i, len = JSValueToNumber(ctx, length, &exc);
	checkerror();

	JSValueRef *argv = calloc (len, sizeof(JSValueRef *));
	if (!argv)
		return NULL;
	for (i = 0; i < len ; i++) {
		argv[i] = JSObjectGetPropertyAtIndex(ctx, argsobj, i, &exc);
		if (!argv[i] || exc) {
			free(argv);
			*flags |= ntEngValFlagException;
			return exc;
		}
	}

	// Call the function
	JSValueRef rval;
	if (JSValueIsUndefined(ctx, ths))
		rval = JSObjectCallAsConstructor (ctx, funcobj, i, argv, &exc);
	else {
		JSObjectRef thsobj = JSValueToObject(ctx, ths, &exc);
		checkerrorval(thsobj);
		rval = JSObjectCallAsFunction (ctx, funcobj, thsobj, i, argv, &exc);
	}
	free (argv);
	checkerror();
	return rval;
}

static ntEngVal jsc_evaluate(const ntEngCtx ctx, ntEngVal ths, const ntEngVal jscript, const ntEngVal filename, unsigned int lineno, ntEngValFlags *flags) {
	JSValueRef exc = NULL;

	JSObjectRef thsobj = JSValueToObject(ctx, ths, &exc);
	checkerrorval(thsobj);

	JSStringRef strjscript = JSValueToStringCopy (ctx, jscript, &exc);
	checkerrorval(strjscript);

	JSStringRef strfilename = JSValueToStringCopy (ctx, filename, &exc);
	if (!strfilename || exc)
		JSStringRelease (strjscript);
	checkerrorval(strfilename);

	JSValueRef rval = JSEvaluateScript (ctx, strjscript, thsobj, strfilename, lineno, &exc);
	JSStringRelease (strjscript);
	JSStringRelease (strfilename);
	checkerror();
	return rval;
}

static ntPrivate *jsc_get_private(const ntEngCtx ctx, const ntEngVal val) {
	JSObjectRef obj = JSValueToObject(ctx, val, NULL);
	if (!obj)
		return NULL;
	return JSObjectGetPrivate (obj);
}

static ntEngVal jsc_get_global(const ntEngCtx ctx, const ntEngVal val) {
	return JSContextGetGlobalObject (ctx);
}

static ntValueType jsc_get_type(const ntEngCtx ctx, const ntEngVal val) {
	JSType type = JSValueGetType (ctx, val);
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

	JSObjectRef obj = JSValueToObject(ctx, val, NULL);
	assert(obj);

	ntPrivate *priv = JSObjectGetPrivate(obj);

	// FUNCTION
	if (JSObjectIsFunction(ctx, obj) && (!priv || JSValueIsObjectOfClass(ctx, val, fnccls)))
		return ntValueTypeFunction;

	// Bypass infinite loops for objects defined by natus
	// (otherwise, the JSObjectGetProperty() calls below will
	//  call the natus defined property handler, which may call
	//  get_type(), etc)
	if (JSObjectGetPrivate (obj))
		return ntValueTypeObject;

	// ARRAY
	/* Yes, this is really ugly. But unfortunately JavaScriptCore is missing JSValueIsArray().
	 * We also can't check the constructor directly for identity with the Array object because
	 * we can't get the global object associated with this value (the global for this context
	 * may not be the global for this value).
	 * Dear JSC, please add JSValueIsArray() (and also JSValueGetGlobal())... */
	JSStringRef str = JSStringCreateWithUTF8CString("constructor");
	if (!str)
		return ntValueTypeObject;
	JSValueRef prp = JSObjectGetProperty(ctx, obj, str, NULL);
	JSStringRelease(str);
	JSObjectRef prpobj = JSValueIsObject(ctx, prp) ? JSValueToObject(ctx, prp, NULL) : NULL;
	str = JSStringCreateWithUTF8CString("name");
	if (!str)
		return ntValueTypeObject;
	if (prpobj && (prp = JSObjectGetProperty(ctx, prpobj, str, NULL))
			&& JSValueIsString(ctx, prp)) {
		JSStringRelease(str);
		str = JSValueToStringCopy(ctx, prp, NULL);
		if (!str)
			return ntValueTypeObject;
		if (JSStringIsEqualToUTF8CString(str, "Array")) {
			JSStringRelease(str);
			return ntValueTypeArray;
		}
	}
	JSStringRelease(str);

	// OBJECT
	return ntValueTypeObject;
}

static bool jsc_borrow_context(ntEngCtx ctx, ntEngVal val, void **context, void **value) {
	*context = (void*) ctx;
	*value = (void*) val;
	return true;
}

static bool jsc_equal(const ntEngCtx ctx, const ntEngVal val1, const ntEngVal val2, ntEqualityStrictness strict) {
	switch (strict) {
		case ntEqualityStrictnessLoose:
			return JSValueIsEqual (ctx, val1, val2, NULL);
		case ntEqualityStrictnessStrict:
			return JSValueIsStrictEqual (ctx, val1, val2);
		case ntEqualityStrictnessIdentity:
			return val1 == val2;
		default:
			assert(false);
	}
}

NATUS_ENGINE("JavaScriptCore", "JSObjectMakeFunctionWithCallback", jsc);

