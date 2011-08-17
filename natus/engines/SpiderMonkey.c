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
#include <string.h>

#include <jsapi.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
typedef JSContext* ntEngCtx;
typedef jsval* ntEngVal;
#include <natus/backend.h>

static void sm_val_free(ntEngCtx ctx, ntEngVal val);
static ntValueType sm_get_type(const ntEngCtx ctx, const ntEngVal val);

static ntEngVal mkjsval(JSContext *ctx, jsval val) {
	jsval *v = malloc(sizeof(jsval));
	if (!v)
		return NULL;
	*v = val;
	JSVAL_LOCK(ctx, *v);
	return v;
}

static void obj_finalize (JSContext *ctx, JSObject *obj);

static ntPrivate *get_private (JSContext *ctx, JSObject *obj) {
	if (!ctx || !obj)
		return NULL;
	if (JS_IsArrayObject (ctx, obj))
		return NULL;

	if (JS_ObjectIsFunction (ctx, obj)) {
		jsval privval;
		if (!JS_GetReservedSlot (ctx, obj, 0, &privval) || !JSVAL_IS_OBJECT (privval))
			return NULL;
		obj = JSVAL_TO_OBJECT (privval);
	}

	JSClass *cls = JS_GetClass (ctx, obj);
	if (!cls || cls->finalize != obj_finalize)
		return NULL;

	return JS_GetPrivate (ctx, obj);
}

static JSBool property_handler (JSContext *ctx, JSObject *object, jsid id, jsval *vp, ntPropertyAction act) {
	ntPrivate *priv = get_private (ctx, object);
	if (!priv)
		return JS_FALSE;

	jsval key;
	if (act & ~ntPropertyActionEnumerate) {
		if (!JS_IdToValue (ctx, id, &key))
			return JS_FALSE;
		assert(JSVAL_IS_STRING(key) || JSVAL_IS_NUMBER(key));
	}

	ntEngValFlags flags = ntEngValFlagNone;
	ntEngVal obj = mkjsval(ctx, OBJECT_TO_JSVAL(object));
	ntEngVal idx = (act & ~ntPropertyActionEnumerate) ? mkjsval(ctx, key) : NULL;
	ntEngVal val = (act & ntPropertyActionSet) ? mkjsval(ctx, *vp) : NULL;
	ntEngVal res = nt_value_handle_property(act, obj, priv, idx, val, &flags);

	if (flags & ntEngValFlagException) {
		if (!res || JSVAL_IS_VOID(*res)) {
			if (flags & ntEngValFlagMustFree && res)
				sm_val_free(ctx, res);
			return JS_TRUE;
		}

		JS_SetPendingException (ctx, *res);
		if (flags & ntEngValFlagMustFree)
			sm_val_free(ctx, res);
		return JS_FALSE;
	}

	if (act == ntPropertyActionGet)
		*vp = *res;

	if (flags & ntEngValFlagMustFree)
		sm_val_free(ctx, res);
	return JS_TRUE;
}

static inline JSBool call_handler (JSContext *ctx, uintN argc, jsval *vp, bool constr) {
	// Allocate our array of arguments
	JSObject *arga = JS_NewArrayObject (ctx, argc, JS_ARGV(ctx, vp));
	if (!arga)
		return JS_FALSE;

	// Get the private
	ntPrivate *priv = get_private (ctx, JSVAL_TO_OBJECT (JS_CALLEE(ctx, vp)));
	if (!priv)
		return JS_FALSE;

	// Do the call
	ntEngValFlags flags;
	ntEngVal obj = mkjsval(ctx, JS_CALLEE(ctx, vp));
	ntEngVal ths = mkjsval(ctx, constr ? JSVAL_VOID : JS_THIS(ctx, vp));
	ntEngVal arg = mkjsval(ctx, OBJECT_TO_JSVAL(arga));
	ntEngVal res = nt_value_handle_call(obj, priv, ths, arg, &flags);

	// Handle the results
	if (flags & ntEngValFlagException) {
		JS_SetPendingException(ctx, res ? *res : JSVAL_VOID);
		if (flags & ntEngValFlagMustFree && res)
			sm_val_free(ctx, res);
		return JS_FALSE;
	}

	JS_SET_RVAL(ctx, vp, *res);
	if (flags & ntEngValFlagMustFree)
		sm_val_free(ctx, res);
	return JS_TRUE;
}

static void obj_finalize (JSContext *ctx, JSObject *obj) {
	nt_private_free (get_private (ctx, obj));
}

static JSBool obj_del (JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	return property_handler (ctx, object, id, vp, ntPropertyActionDelete);
}

static JSBool obj_get (JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	return property_handler (ctx, object, id, vp, ntPropertyActionGet);
}

static JSBool obj_set (JSContext *ctx, JSObject *object, jsid id, JSBool strict, jsval *vp) {
	return property_handler (ctx, object, id, vp, ntPropertyActionSet);
}

static JSBool obj_enum (JSContext *ctx, JSObject *object, JSIterateOp enum_op, jsval *statep, jsid *idp) {
	jsuint len;
	jsval step;
	if (enum_op == JSENUMERATE_NEXT) {
		assert(statep && idp);
		// Get our length and the current step
		if (!JS_GetArrayLength (ctx, JSVAL_TO_OBJECT (*statep), &len))
			return JS_FALSE;
		if (!JS_GetProperty (ctx, JSVAL_TO_OBJECT (*statep), "step", &step))
			return JS_FALSE;

		// If we have finished our iteration, end
		if ((jsuint) JSVAL_TO_INT (step) >= len) {
			JSVAL_UNLOCK(ctx, *statep);
			*statep = JSVAL_NULL;
			return JS_TRUE;
		}

		// Get the item
		jsval item;
		if (!JS_GetElement (ctx, JSVAL_TO_OBJECT (*statep), JSVAL_TO_INT (step), &item))
			return JS_FALSE;
		JS_ValueToId (ctx, item, idp);

		// Increment to the next step
		step = INT_TO_JSVAL (JSVAL_TO_INT (step) + 1);
		if (!JS_SetProperty (ctx, JSVAL_TO_OBJECT (*statep), "step", &step))
			return JS_FALSE;
		return JS_TRUE;
	} else if (enum_op == JSENUMERATE_DESTROY) {
		assert(statep);
		JSVAL_UNLOCK(ctx, *statep);
		return JS_TRUE;
	}
	assert(enum_op == JSENUMERATE_INIT);
	assert(statep);

	// Handle the results
	jsval res = JSVAL_VOID;
	step = INT_TO_JSVAL (0);
	if (!property_handler(ctx, object, 0, &res, ntPropertyActionEnumerate)
		|| sm_get_type(ctx, &res) != ntValueTypeArray
		|| !JS_GetArrayLength (ctx, JSVAL_TO_OBJECT (res), &len)
		|| !JS_SetProperty (ctx, JSVAL_TO_OBJECT (res), "step", &step))
		return JS_FALSE;

	// Keep the jsval, but dispose of the wrapper
	*statep = res;
	JSVAL_LOCK(ctx, *statep);
	if (idp)
		*idp = INT_TO_JSVAL (len);
	return JS_TRUE;
}

static JSBool obj_call (JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler (ctx, argc, vp, false);
}

static JSBool obj_new (JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler (ctx, argc, vp, true);
}

static JSBool fnc_call (JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler (ctx, argc, vp, JS_IsConstructing (ctx, vp));
}

static JSClass glbdef = { "GlobalObject", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static JSClass fncdef = { "NativeFunction", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static JSClass objdef = { "NativeObject", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static void report_error (JSContext *cx, const char *message, JSErrorReport *report) {
	fprintf (stderr, "%s:%u:%s\n", report->filename ? report->filename : "<no filename>", (unsigned int) report->lineno, message);
}

static void sm_ctx_free(ntEngCtx ctx) {
	JSRuntime *run = JS_GetRuntime(ctx);
	JS_GC (ctx);
	JS_DestroyContext (ctx);
	JS_DestroyRuntime (run);
}

static void sm_val_free(ntEngCtx ctx, ntEngVal val) {
	JSVAL_UNLOCK(ctx, *val);
	free(val);
}

static ntEngVal sm_new_global(ntEngCtx ctx, ntEngVal val, ntPrivate *priv, ntEngCtx *newctx, ntEngValFlags *flags) {
	bool freectx = false;
	JSObject *glb = NULL;

	/* Reuse existing runtime and context */
	if (ctx && val) {
		*newctx = ctx;
		glb = JS_NewGlobalObject(ctx, &glbdef);

	/* Create a new runtime and context */
	} else {
		freectx = true;

		JSRuntime *run = JS_NewRuntime (1024 * 1024);
		if (!run)
			return NULL;

		*newctx = JS_NewContext (run, 1024 * 1024);
		if (!*newctx) {
			JS_DestroyRuntime(run);
			return NULL;
		}
		JS_SetOptions (*newctx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_XML | JSOPTION_UNROOTED_GLOBAL);
		JS_SetVersion (*newctx, JSVERSION_LATEST);
		JS_SetErrorReporter (*newctx, report_error);

		glb = JS_NewCompartmentAndGlobalObject (*newctx, &glbdef, NULL);
	}
	if (!glb || !JS_InitStandardClasses (*newctx, glb)) {
		if (freectx && *newctx) {
			JSRuntime *run = JS_GetRuntime(*newctx);
			JS_DestroyContext(*newctx);
			JS_DestroyRuntime(run);
		}
		return NULL;
	}

	if (!JS_SetPrivate (ctx, glb, priv)) {
		nt_private_free(priv);
		if (freectx && *newctx) {
			JSRuntime *run = JS_GetRuntime(*newctx);
			JS_DestroyContext(*newctx);
			JS_DestroyRuntime(run);
		}
		return NULL;
	}

	ntEngVal v = mkjsval(*newctx, OBJECT_TO_JSVAL(glb));
	if (!v) {
		if (freectx && *newctx) {
			JSRuntime *run = JS_GetRuntime(*newctx);
			JS_DestroyContext(*newctx);
			JS_DestroyRuntime(run);
		}
		return NULL;
	}
	return v;
}

static ntEngVal sm_new_bool(const ntEngCtx ctx, bool b, ntEngValFlags *flags) {
	return mkjsval(ctx, BOOLEAN_TO_JSVAL(b));
}

static ntEngVal sm_new_number(const ntEngCtx ctx, double n, ntEngValFlags *flags) {
	jsval v = JSVAL_VOID;
	if (!JS_NewNumberValue(ctx, n, &v)) {
		v = JSVAL_VOID;
		if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
			*flags |= ntEngValFlagException;
	}
	return mkjsval(ctx, v);
}

static ntEngVal sm_new_string_utf8(const ntEngCtx ctx, const char *str, size_t len, ntEngValFlags *flags) {
	jsval v = JSVAL_VOID;
	JSString *s = JS_NewStringCopyN(ctx, str, len);
	if (s)
		v = STRING_TO_JSVAL(s);
	else {
		if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
			*flags |= ntEngValFlagException;
	}

	return mkjsval(ctx, STRING_TO_JSVAL(s));
}

static ntEngVal sm_new_string_utf16(const ntEngCtx ctx, const ntChar *str, size_t len, ntEngValFlags *flags) {
	jsval v = JSVAL_VOID;
	JSString *s = JS_NewUCStringCopyN(ctx, str, len);
	if (s)
		v = STRING_TO_JSVAL(s);
	else {
		if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
			*flags |= ntEngValFlagException;
	}

	return mkjsval(ctx, STRING_TO_JSVAL(s));
}

static ntEngVal sm_new_array(const ntEngCtx ctx, const ntEngVal *array, size_t len, ntEngValFlags *flags) {
	jsval *valv = calloc (len, sizeof(jsval));
	if (!valv)
		return NULL;

	int i;
	for (i = 0; i < len ; i++)
		valv[i] = *array[i];

	JSObject* obj = JS_NewArrayObject(ctx, i, valv);
	free (valv);

	jsval v;
	if (obj)
		v = OBJECT_TO_JSVAL(obj);
	else if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
		*flags |= ntEngValFlagException;

	return mkjsval(ctx, v);
}

static ntEngVal sm_new_function(const ntEngCtx ctx, const char *name, ntPrivate *priv, ntEngValFlags *flags) {
	JSFunction *fnc = JS_NewFunction (ctx, fnc_call, 0, JSFUN_CONSTRUCTOR, NULL, name);
	JSObject *obj = JS_GetFunctionObject (fnc);
	JSObject *prv = JS_NewObject (ctx, &fncdef, NULL, NULL);

	jsval v = JSVAL_VOID;
	if (fnc && obj && prv && JS_SetReservedSlot (ctx, obj, 0, OBJECT_TO_JSVAL (prv)) && JS_SetPrivate (ctx, prv, priv))
		v = OBJECT_TO_JSVAL(obj);
	else {
		nt_private_free(priv);
		if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &v))
			return NULL;
		*flags |= ntEngValFlagException;
	}

	return mkjsval(ctx, v);
}

static ntEngVal sm_new_object(const ntEngCtx ctx, ntClass *cls, ntPrivate *priv, ntEngValFlags *flags) {
	JSClass *jscls = NULL;
	if (cls) {
		jscls = malloc (sizeof(JSClass));
		if (!jscls)
			goto error;

		memset (jscls, 0, sizeof(JSClass));
		jscls->name = cls->call ? "NativeFunction" : "NativeObject";
		jscls->flags = cls->enumerate ? JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE : JSCLASS_HAS_PRIVATE;
		jscls->addProperty = JS_PropertyStub;
		jscls->delProperty = cls->del ? obj_del : JS_PropertyStub;
		jscls->getProperty = cls->get ? obj_get : JS_PropertyStub;
		jscls->setProperty = cls->set ? obj_set : JS_StrictPropertyStub;
		jscls->enumerate = cls->enumerate ? (JSEnumerateOp) obj_enum : JS_EnumerateStub;
		jscls->resolve = JS_ResolveStub;
		jscls->convert = JS_ConvertStub;
		jscls->finalize = obj_finalize;
		jscls->call = cls->call ? obj_call : NULL;
		jscls->construct = cls->call ? obj_new : NULL;

		if (!nt_private_set (priv, "natus::SpiderMonkey::JSClass", jscls, free))
			goto error;
	}

	// Build the object
	jsval v = JSVAL_VOID;
	JSObject* obj = JS_NewObject (ctx, jscls ? jscls : &objdef, NULL, NULL);
	if (obj && JS_SetPrivate (ctx, obj, priv))
		v = OBJECT_TO_JSVAL(obj);
	else {
		if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &v))
			goto error;
		*flags |= ntEngValFlagException;
		nt_private_free(priv);
	}

	return mkjsval(ctx, v);

	error:
		nt_private_free(priv);
		return NULL;
}

static ntEngVal sm_new_null(const ntEngCtx ctx, ntEngValFlags *flags) {
	return mkjsval(ctx, JSVAL_NULL);
}

static ntEngVal sm_new_undefined(const ntEngCtx ctx, ntEngValFlags *flags) {
	return mkjsval(ctx, JSVAL_VOID);
}

static bool sm_to_bool(const ntEngCtx ctx, const ntEngVal val) {
	return JSVAL_TO_BOOLEAN(*val);
}

static double sm_to_double(const ntEngCtx ctx, const ntEngVal val) {
	double d;
	JS_ValueToNumber (ctx, *val, &d);
	return d;
}

static char *sm_to_string_utf8(const ntEngCtx ctx, const ntEngVal val, size_t *len) {
	JSString *str = JS_ValueToString (ctx, *val);
	*len = JS_GetStringLength (str);

	size_t bufflen = JS_GetStringEncodingLength (ctx, str);
	char *buff = calloc (bufflen + 1, sizeof(char));
	if (!buff)
		return NULL;
	memset (buff, 0, sizeof(char) * (bufflen + 1));

	*len = JS_EncodeStringToBuffer (str, buff, bufflen);
	if (*len >= 0 && *len <= bufflen)
		return buff;
	free (buff);
	return NULL;
}

static ntChar *sm_to_string_utf16(const ntEngCtx ctx, const ntEngVal val, size_t *len) {
	JSString *str = JS_ValueToString(ctx, *val);
	*len = JS_GetStringLength(str);

	const jschar *jschars = JS_GetStringCharsAndLength(ctx, str, len);
	ntChar *ntchars = calloc(*len + 1, sizeof(ntChar));
	if (jschars && ntchars) {
		memset(ntchars, 0, sizeof(ntChar) * (*len + 1));
		memcpy(ntchars, jschars, sizeof(ntChar) * *len);
		return ntchars;
	}

	free(ntchars);
	return NULL;
}

ntEngVal sm_del(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags) {
	jsid vid;
	jsval rval = JSVAL_VOID;

	JS_ValueToId (ctx, *id, &vid);

	if (JS_DeletePropertyById (ctx, JSVAL_TO_OBJECT(*val), vid))
		rval = BOOLEAN_TO_JSVAL(true);
	else {
		*flags |= ntEngValFlagException;
		if (!JS_IsExceptionPending (ctx) || !JS_GetPendingException (ctx, &rval))
			return NULL;
	}

	return mkjsval(ctx, rval);
}

ntEngVal sm_get(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags) {
	jsid vid;
	JS_ValueToId (ctx, *id, &vid);

	jsval rval = JSVAL_VOID;
	if (!JS_GetPropertyById (ctx, JSVAL_TO_OBJECT(*val), vid, &rval)) {
		*flags |= ntEngValFlagException;
		if (!JS_IsExceptionPending (ctx) || !JS_GetPendingException (ctx, &rval))
			return NULL;
	}

	return mkjsval(ctx, rval);
}

ntEngVal sm_set(const ntEngCtx ctx, ntEngVal val, const ntEngVal id, const ntEngVal value, ntPropAttr attrs, ntEngValFlags *flags) {
	jsint attr = (attrs & ntPropAttrDontEnum) ? 0 : JSPROP_ENUMERATE;
	attr |= (attrs & ntPropAttrReadOnly) ? JSPROP_READONLY : 0;
	attr |= (attrs & ntPropAttrDontDelete) ? JSPROP_PERMANENT : 0;

	jsid vid;
	JS_ValueToId (ctx, *id, &vid);

	jsval rval = *value;
	if (JS_SetPropertyById (ctx, JSVAL_TO_OBJECT(*val), vid, &rval)) {
		BOOLEAN_TO_JSVAL(true);

		if (JSID_IS_STRING (vid)) {
			size_t len;
			JSBool found;
			JSString *str = JSID_TO_STRING (vid);
			const jschar *chars = JS_GetStringCharsAndLength (ctx, str, &len);
			JS_SetUCPropertyAttributes (ctx, JSVAL_TO_OBJECT(*val), chars, len, attr, &found);
		}
	} else {
		*flags |= ntEngValFlagException;
		if (!JS_IsExceptionPending (ctx) || !JS_GetPendingException (ctx, &rval))
			return NULL;
	}

	return mkjsval(ctx, rval);
}

ntEngVal sm_enumerate(const ntEngCtx ctx, ntEngVal val, ntEngValFlags *flags) {
	jsint i;

	JSIdArray* na = JS_Enumerate (ctx, JSVAL_TO_OBJECT(*val));
	if (!na)
		return NULL;

	jsval *vals = calloc(na->length, sizeof(jsval));
	if (!vals) {
		JS_DestroyIdArray (ctx, na);
		return NULL;
	}

	for (i = 0; i < na->length ; i++)
		assert(JS_IdToValue (ctx, na->vector[i], &vals[i]));

	JSObject *array = JS_NewArrayObject(ctx, na->length, vals);
	JS_DestroyIdArray (ctx, na);
	free(vals);
	if (!array)
		return NULL;

	return mkjsval(ctx, OBJECT_TO_JSVAL(array));
}

static ntEngVal sm_call(const ntEngCtx ctx, ntEngVal func, ntEngVal ths, ntEngVal args, ntEngValFlags *flags) {
	// Convert to jsval array
	jsuint i, len;
	if (!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(*args), &len))
		return NULL;

	jsval *argv = calloc (len, sizeof(jsval));
	if (!argv)
		return NULL;


	for (i = 0; i < len ; i++) {
		jsid id;
		if (!JS_NewNumberValue(ctx, i, &argv[i])
			|| !JS_ValueToId(ctx, argv[i], &id)
			|| !JS_GetPropertyById(ctx, JSVAL_TO_OBJECT(*args), id, &argv[i])) {
			free(argv);
			return NULL;
		}
	}

	// Call the function
	jsval rval = JSVAL_VOID;
	if (JSVAL_IS_VOID(*ths)) {
		JSObject *obj = JS_New (ctx, JSVAL_TO_OBJECT(*func), len, argv);
		if (obj)
			rval = OBJECT_TO_JSVAL (obj);
		else {
			*flags |= ntEngValFlagException;
			if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
				free(argv);
				return NULL;
			}
		}
	} else {
		if (!JS_CallFunctionValue (ctx, JSVAL_TO_OBJECT(*ths), *func, len, argv, &rval)) {
			*flags |= ntEngValFlagException;
			if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
				free(argv);
				return NULL;
			}
		}
	}
	free (argv);

	return mkjsval(ctx, rval);
}

static ntEngVal sm_evaluate(const ntEngCtx ctx, ntEngVal ths, const ntEngVal jscript, const ntEngVal filename, unsigned int lineno, ntEngValFlags *flags) {
	size_t jslen = 0, fnlen = 0;
	const jschar *jschars = JS_GetStringCharsAndLength (ctx, JS_ValueToString (ctx, *jscript), &jslen);
	char *fnchars = sm_to_string_utf8(ctx, filename, &fnlen);

	jsval rval;
	if (!JS_EvaluateUCScript (ctx, JSVAL_TO_OBJECT(*ths), jschars, jslen, fnchars, lineno, &rval)) {
		*flags |= ntEngValFlagException;
		if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
			free(fnchars);
			return NULL;
		}
	}

	free (fnchars);
	return mkjsval(ctx, rval);
}

static ntPrivate *sm_get_private(const ntEngCtx ctx, const ntEngVal val) {
	return get_private (ctx, JSVAL_TO_OBJECT(*val));
}

static ntEngVal sm_get_global(const ntEngCtx ctx, const ntEngVal val) {
	JSObject *obj = NULL;
	JS_ValueToObject(ctx, *val, &obj);
	if (obj)
		obj = JS_GetGlobalForObject(ctx, obj);
	if (!obj)
		obj = JS_GetGlobalForScopeChain(ctx);
	if (!obj)
		obj = JS_GetGlobalObject(ctx);
	return mkjsval(ctx, OBJECT_TO_JSVAL(obj));
}

static ntValueType sm_get_type(const ntEngCtx ctx, const ntEngVal val) {
	if (JSVAL_IS_BOOLEAN (*val))
		return ntValueTypeBoolean;
	else if (JSVAL_IS_NULL (*val))
		return ntValueTypeNull;
	else if (JSVAL_IS_NUMBER (*val))
		return ntValueTypeNumber;
	else if (JSVAL_IS_STRING (*val))
		return ntValueTypeString;
	else if (JSVAL_IS_VOID (*val))
		return ntValueTypeUndefined;
	else if (JSVAL_IS_OBJECT (*val)) {
		if (JS_IsArrayObject (ctx, JSVAL_TO_OBJECT (*val)))
			return ntValueTypeArray;
		else if (JS_ObjectIsFunction (ctx, JSVAL_TO_OBJECT (*val)))
			return ntValueTypeFunction;
		else
			return ntValueTypeObject;
	}
	return ntValueTypeUnknown;
}

static bool sm_borrow_context(ntEngCtx ctx, ntEngVal val, void **context, void **value) {
	*context = ctx;
	*value = val;
	return true;
}

static bool sm_equal(const ntEngCtx ctx, const ntEngVal val1, const ntEngVal val2, ntEqualityStrictness strict) {
	JSBool eql = false;

	switch (strict) {
		default:
		case ntEqualityStrictnessLoose:
#if 0
			if (!JS_LooselyEqual (ctx, *val1, *val2, &eql))
				return false;
			break;
#endif
		case ntEqualityStrictnessStrict:
			if (!JS_StrictlyEqual (ctx, *val1, *val2, &eql))
				return false;
			break;
		case ntEqualityStrictnessIdentity:
			eql = (!val1 && !val2) || (val1 && val2 && *val1 == *val2);
			break;
	}

	return eql;
}

__attribute__((constructor))
static void _init() {
	JS_SetCStringsAreUTF8();
}

__attribute__((destructor))
static void _fini() {
	JS_ShutDown();
}

NATUS_ENGINE("SpiderMonkey", "JS_GetProperty", sm);
