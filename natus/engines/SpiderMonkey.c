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
#include <natus/backend.h>

#define CTX(v) (((smValue*) v)->ctx)
#define VAL(v) (((smValue*) v)->val)
#define OBJ(v) (JSVAL_TO_OBJECT(VAL(v)))

typedef struct {
	ntValue     hdr;
	JSContext  *ctx;
	jsval       val;
} smValue;

typedef enum {
	smPropertyActionDel,
	smPropertyActionGet,
	smPropertyActionSet
} smPropertyAction;

static void obj_finalize(JSContext *ctx, JSObject *obj);

static inline ntPrivate *get_private(JSContext *ctx, JSObject *obj) {
	if (!ctx || !obj) return NULL;
	if (JS_IsArrayObject(ctx, obj)) return NULL;

	if (JS_ObjectIsFunction(ctx, obj)) {
		jsval privval;
		if (!JS_GetReservedSlot(ctx, obj, 0, &privval) || !JSVAL_IS_OBJECT(privval))
			return NULL;
		obj = JSVAL_TO_OBJECT(privval);
	}

	JSClass *cls = JS_GetClass(ctx, obj);
	if (!cls || cls->finalize != obj_finalize) return NULL;

	return JS_GetPrivate(ctx, obj);
}

static inline ntPrivate *get_private_val(JSContext *ctx, jsval val) {
	if (JSVAL_IS_NULL(val) || !JSVAL_IS_OBJECT(val)) return NULL;
	return get_private(ctx, JSVAL_TO_OBJECT(val));
}

static ntValue* get_instance(const ntValue* ctx, jsval val, bool exc)  {
	ntValue *global = nt_private_get(get_private_val(CTX(ctx), val), NATUS_PRIV_GLOBAL);
	if (global && VAL(global) == val) return nt_value_incref(global);

	ntValue *self = malloc(sizeof(smValue));
	if (!self) return self;
	memset(self, 0, sizeof(smValue));

	CTX(self) = CTX(ctx);
	VAL(self) = val;
	self->typ = _ntValueTypeUnknown;
	JSVAL_LOCK(CTX(ctx), val);

	self->eng = nt_engine_incref(ctx->eng);
	self->exc = exc;
	self->ref = 1;
	return self;
}

static inline JSBool property_handler(JSContext *ctx, JSObject *object, jsid id, jsval *vp, smPropertyAction act) {
	ntPrivate *priv = get_private(ctx, object);
	ntValue   *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	ntClass   *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
	assert(glbl && clss);

	jsval key;
	JS_IdToValue(ctx, id, &key);
	ntValue *obj = get_instance(glbl, OBJECT_TO_JSVAL(object), false);
	ntValue *idx = get_instance(glbl, key, false);
	ntValue *val = NULL;
	assert(nt_value_is_type(idx, ntValueTypeNumber | ntValueTypeString));
	ntValue *res = NULL;
	switch (act) {
		case smPropertyActionDel:
			assert(clss->del);
			res = clss->del(clss, obj, idx);
			break;
		case smPropertyActionGet:
			assert(clss->get);
			res = clss->get(clss, obj, idx);
			break;
		case smPropertyActionSet:
			assert(clss->set);
			val = get_instance(glbl, *vp, false);
			res = clss->set(clss, obj, idx, val);
			break;
		default:
			assert(false);
	}
	nt_value_decref(obj);
	nt_value_decref(idx);
	nt_value_decref(val);

	if (nt_value_is_exception(res)) {
		if (res)
			JS_SetPendingException(ctx, VAL(res));
		else
			JS_ReportOutOfMemory(ctx);
		nt_value_decref(res);
		return JS_FALSE;
	}
	if (act == smPropertyActionGet) *vp = VAL(res);
	nt_value_decref(res);
	return JS_TRUE;
}

static inline JSBool call_handler(JSContext *ctx, uintN argc, jsval *vp, bool constr) {
	ntPrivate       *priv = get_private(ctx, JSVAL_TO_OBJECT(JS_CALLEE(ctx, vp)));
	ntValue         *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	ntNativeFunction func = nt_private_get(priv, NATUS_PRIV_FUNCTION);
	ntClass         *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
	assert(glbl && (func || clss));

	// Allocate our array of arguments
	JSObject *arga = JS_NewArrayObject(ctx, argc, JS_ARGV(ctx, vp));
	if (!arga) return JS_FALSE;

	// Do the call
	ntValue *fnc = get_instance(glbl, JS_CALLEE(ctx, vp), false);
	ntValue *ths = constr ? nt_value_new_undefined(glbl) : get_instance(glbl, JS_THIS(ctx, vp), false);
	ntValue *arg = get_instance(glbl, OBJECT_TO_JSVAL(arga), false);
	ntValue *res = func ? func(fnc, ths, arg) : clss->call(clss, fnc, ths, arg);
	nt_value_decref(arg);
	nt_value_decref(ths);
	nt_value_decref(fnc);

	// Handle results
	if (nt_value_is_exception(res)) {
		JS_SetPendingException(ctx, VAL(res));
		nt_value_decref(res);
		return JS_FALSE;
	}
	JS_SET_RVAL(ctx, vp, VAL(res));
	nt_value_decref(res);
	return JS_TRUE;
}

static JSBool obj_convert(JSContext *ctx, JSObject *obj, JSType type, jsval *vp) {
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

static void obj_finalize(JSContext *ctx, JSObject *obj) {
	nt_private_free(get_private(ctx, obj));
}

static JSBool obj_del(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	return property_handler(ctx, object, id, vp, smPropertyActionDel);
}

static JSBool obj_get(JSContext *ctx, JSObject *object, jsid id, jsval *vp) {
	return property_handler(ctx, object, id, vp, smPropertyActionGet);
}

static JSBool obj_set(JSContext *ctx, JSObject *object, jsid id, JSBool strict, jsval *vp) {
	return property_handler(ctx, object, id, vp, smPropertyActionSet);
}

static JSBool obj_enum(JSContext *ctx, JSObject *object, JSIterateOp enum_op, jsval *statep, jsid *idp) {
	jsuint len;
	jsval step;
	if (enum_op == JSENUMERATE_NEXT) {
		assert(statep && idp);
		// Get our length and the current step
		if (!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(*statep), &len)) return JS_FALSE;
		if (!JS_GetProperty(ctx, JSVAL_TO_OBJECT(*statep), "step", &step)) return JS_FALSE;

		// If we have finished our iteration, end
		if ((jsuint) JSVAL_TO_INT(step) >= len) {
			JSVAL_UNLOCK(ctx, *statep);
			*statep = JSVAL_NULL;
			return JS_TRUE;
		}

		// Get the item
		jsval item;
		if (!JS_GetElement(ctx, JSVAL_TO_OBJECT(*statep), JSVAL_TO_INT(step), &item)) return JS_FALSE;
		JS_ValueToId(ctx, item, idp);

		// Increment to the next step
		step = INT_TO_JSVAL(JSVAL_TO_INT(step) + 1);
		if (!JS_SetProperty(ctx, JSVAL_TO_OBJECT(*statep), "step", &step)) return JS_FALSE;
		return JS_TRUE;
	} else if (enum_op == JSENUMERATE_DESTROY) {
		assert(statep);
		JSVAL_UNLOCK(ctx, *statep);
		return JS_TRUE;
	}
	assert(enum_op == JSENUMERATE_INIT);
	assert(statep);

	// Get our privates
	ntPrivate *priv = get_private(ctx, object);
	ntValue   *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	ntClass   *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
	assert(glbl && clss && clss->set);

	// Call our callback
	ntValue *obj = get_instance(glbl, OBJECT_TO_JSVAL(object), false);
	ntValue *res = clss->enumerate(clss, obj);
	nt_value_decref(obj);

	// Handle the results
	step = INT_TO_JSVAL(0);
	if (!nt_value_is_array(res) ||
		!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(VAL(res)), &len) ||
		!JS_SetProperty(ctx, JSVAL_TO_OBJECT(VAL(res)), "step", &step)) {
		nt_value_decref(res);
		return JS_FALSE;
	}

	// Keep the jsval, but dispose of the ntValue
	*statep = VAL(res);
	nt_value_decref(res);
	JSVAL_LOCK(ctx, *statep);
	if (idp) *idp = INT_TO_JSVAL(len);
	return JS_TRUE;
}

static JSBool obj_call(JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler(ctx, argc, vp, false);
}

static JSBool obj_new(JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler(ctx, argc, vp, true);
}

static JSBool fnc_call(JSContext *ctx, uintN argc, jsval *vp) {
	return call_handler(ctx, argc, vp, JS_IsConstructing(ctx, vp));
}

static JSClass glbdef = { "GlobalObject", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static JSClass fncdef = { "NativeFunction", JSCLASS_HAS_PRIVATE,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub, obj_convert, obj_finalize, };

static JSClass objdef = { "NativeObject", JSCLASS_HAS_PRIVATE,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub, obj_convert, obj_finalize, };

static void report_error(JSContext *cx, const char *message,
		JSErrorReport *report) {
	fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename
			: "<no filename>", (unsigned int) report->lineno, message);
}

ntPrivate       *sm_value_private_get      (const ntValue *val) {
	return get_private(CTX(val), OBJ(val));
}

static bool             sm_value_borrow_context   (const ntValue *ctx, void **context, void **value) {
	if (context) *context = &(CTX(ctx));
	if (value)   *value = &(VAL(ctx));
	return true;
}

static ntValue*         sm_value_get_global       (const ntValue *val) {
	return (ntValue*) nt_private_get(get_private(CTX(val), JS_GetGlobalObject(CTX(val))), NATUS_PRIV_GLOBAL);
}

static ntValueType      sm_value_get_type         (const ntValue *val) {
	if (JSVAL_IS_BOOLEAN(VAL(val)))
		return ntValueTypeBool;
	else if(JSVAL_IS_NULL(VAL(val)))
		return ntValueTypeNull;
	else if(JSVAL_IS_NUMBER(VAL(val)))
		return ntValueTypeNumber;
	else if (JSVAL_IS_STRING(VAL(val)))
		return ntValueTypeString;
	else if (JSVAL_IS_VOID(VAL(val)))
		return ntValueTypeUndefined;
	else if (JSVAL_IS_OBJECT(VAL(val))) {
		if (JS_IsArrayObject(CTX(val), JSVAL_TO_OBJECT(VAL(val))))
			return ntValueTypeArray;
		else if (JS_ObjectIsFunction(CTX(val), JSVAL_TO_OBJECT(VAL(val))))
			return ntValueTypeFunction;
		else
			return ntValueTypeObject;
	}
	return _ntValueTypeUnknown;
}

void             sm_value_free             (ntValue *val) {
	JSVAL_UNLOCK(CTX(val), VAL(val));
	if (JS_GetGlobalObject(CTX(val)) == OBJ(val)) {
		JS_GC(CTX(val));
		JS_DestroyContext(CTX(val));
	}
	nt_engine_decref(val->eng);
	free(val);
}

ntValue*         sm_value_new_bool         (const ntValue *ctx, bool b) {
	return get_instance(ctx, BOOLEAN_TO_JSVAL(b), false);
}

ntValue*         sm_value_new_number       (const ntValue *ctx, double n) {
	jsval v;
	assert(JS_NewNumberValue(CTX(ctx), n, &v));
	return get_instance(ctx, v, false);
}

ntValue*         sm_value_new_string_utf8  (const ntValue *ctx, const char   *str, size_t len) {
	return get_instance(ctx, STRING_TO_JSVAL(JS_NewStringCopyN(CTX(ctx), str, len)), false);
}

ntValue*         sm_value_new_string_utf16 (const ntValue *ctx, const ntChar *str, size_t len) {
	return get_instance(ctx, STRING_TO_JSVAL(JS_NewUCStringCopyN(CTX(ctx), str, len)), false);
}

ntValue*         sm_value_new_array        (const ntValue *ctx, const ntValue **array) {
	int i;
	for (i=0 ; array && array[i] ; i++);

	jsval *valv = calloc(i, sizeof(jsval));
	if (!valv) return NULL;
	for (i=0 ; array && array[i] ; i++)
		valv[i] = VAL(array[i]);

	JSObject* obj = JS_NewArrayObject(CTX(ctx), i, valv);
	free(valv);
	return get_instance(ctx, OBJECT_TO_JSVAL(obj), false);
}

ntValue*         sm_value_new_function     (const ntValue *ctx, ntPrivate *priv) {
	JSFunction *fnc = JS_NewFunction(CTX(ctx), fnc_call, 0, JSFUN_CONSTRUCTOR, NULL, NULL);
	JSObject   *obj = JS_GetFunctionObject(fnc);
	JSObject   *prv = JS_NewObject(CTX(ctx), &fncdef, NULL, NULL);
	if (!fnc || !obj || !prv || !JS_SetReservedSlot(CTX(ctx), obj, 0, OBJECT_TO_JSVAL(prv)))
		return NULL;

	if (!JS_SetPrivate(CTX(ctx), prv, priv))
		return NULL;

	return get_instance(ctx, OBJECT_TO_JSVAL(obj), false);
}

ntValue*         sm_value_new_object       (const ntValue *ctx, ntClass *cls, ntPrivate *priv) {
	JSClass *jscls = NULL;
	if (cls) {
		jscls = malloc(sizeof(JSClass));
		if (!jscls) return NULL;

		memset(jscls, 0, sizeof(JSClass));
		jscls->name        = cls->call      ? "NativeFunction" : "NativeObject";
		jscls->flags       = cls->enumerate ? JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE : JSCLASS_HAS_PRIVATE;
		jscls->addProperty = JS_PropertyStub;
		jscls->delProperty = cls->del       ? obj_del : JS_PropertyStub;
		jscls->getProperty = cls->get       ? obj_get : JS_PropertyStub;
		jscls->setProperty = cls->set       ? obj_set : JS_StrictPropertyStub;
		jscls->enumerate   = cls->enumerate ? (JSEnumerateOp) obj_enum : JS_EnumerateStub;
		jscls->resolve     = JS_ResolveStub;
		jscls->convert     = obj_convert;
		jscls->finalize    = obj_finalize;
		jscls->call        = cls->call      ? obj_call : NULL;
		jscls->construct   = cls->call      ? obj_new : NULL;

		if (!nt_private_set(priv, "natus.SpiderMonkey.JSClass", jscls, free))
			return NULL;
	}

	// Build the object
	JSObject* obj = JS_NewObject(CTX(ctx), jscls ? jscls : &objdef, NULL, NULL);
	if (!obj || !JS_SetPrivate(CTX(ctx), obj, priv))
		return NULL;
	return get_instance(ctx, OBJECT_TO_JSVAL(obj), false);
}

ntValue*         sm_value_new_null         (const ntValue *ctx) {
	return get_instance(ctx, JSVAL_NULL, false);
}

ntValue*         sm_value_new_undefined    (const ntValue *ctx) {
	return get_instance(ctx, JSVAL_VOID, false);
}

static bool             sm_value_to_bool          (const ntValue *val) {
	return JSVAL_TO_BOOLEAN(VAL(val));
}

static double           sm_value_to_double        (const ntValue *val) {
	double d;
	JS_ValueToNumber(CTX(val), VAL(val), &d);
	return d;
}

static inline JSString *_val_to_string(JSContext *ctx, jsval val) {
	JSClass* jsc = NULL;
	jsval v;
	if (!JSVAL_IS_OBJECT(val)
			|| !(jsc = JS_GetClass(ctx, JSVAL_TO_OBJECT(val)))
			|| !jsc->convert
			|| !jsc->convert(ctx, JSVAL_TO_OBJECT(val), JSTYPE_STRING, &v))
		JS_ConvertValue(ctx, val, JSTYPE_STRING, &v);

	if (JSVAL_IS_OBJECT(v))
		return JS_NewStringCopyN(ctx, "[object Object]", strlen("[object Object]"));
	return JS_ValueToString(ctx, v);
}

static char            *sm_value_to_string_utf8   (const ntValue *val, size_t *len) {
	JSString *str = _val_to_string(CTX(val), VAL(val));
	*len = JS_GetStringLength(str);

	size_t bufflen = JS_GetStringEncodingLength(CTX(val), str);
	char  *buff = calloc(bufflen+1, sizeof(char));
	if (!buff) return NULL;
	memset(buff, 0, sizeof(char) * (bufflen+1));

	*len = JS_EncodeStringToBuffer(str, buff, bufflen);
	if (*len >= 0 && *len <= bufflen) return buff;
	free(buff);
	return NULL;
}

static ntChar          *sm_value_to_string_utf16  (const ntValue *val, size_t *len) {
	JSString *str = _val_to_string(CTX(val), VAL(val));
	*len = JS_GetStringLength(str);

	const jschar *jschars = JS_GetStringCharsAndLength(CTX(val), str, len);
	      ntChar *ntchars = calloc(*len, sizeof(ntChar));
	if (jschars && ntchars) {
		memcpy(ntchars, jschars, sizeof(ntChar) * *len);
		return ntchars;
	}

	free(ntchars);
	return NULL;
}

static ntValue         *sm_value_del        (ntValue *obj, const ntValue *idx) {
	jsid vid;
	JS_ValueToId(CTX(idx), VAL(idx), &vid);

	JSClass* jsc = NULL;
	if (!(jsc = JS_GetClass(CTX(obj), OBJ(obj)))
			|| !jsc->delProperty
			||  jsc->delProperty == JS_PropertyStub
			|| !jsc->delProperty(CTX(obj), OBJ(obj), vid, NULL)) {
		if (nt_value_is_number(idx)
				? !JS_DeleteElement(CTX(obj), OBJ(obj), nt_value_to_long(idx))
				: !JS_DeletePropertyById(CTX(obj), OBJ(obj), vid)) {
			jsval rval;
			if (JS_IsExceptionPending(CTX(obj)) && JS_GetPendingException(CTX(obj), &rval))
				return get_instance(obj, rval, true);
			return sm_value_new_bool(obj, false);
		}
	}
	return sm_value_new_bool(obj, true);
}

static ntValue         *sm_value_get        (const ntValue *obj, const ntValue *idx) {
	jsid vid;
	JS_ValueToId(CTX(idx), VAL(idx), &vid);

	bool isexc = false;
	jsval v = JSVAL_VOID;
	JSClass* jsc = NULL;
	if (!(jsc = JS_GetClass(CTX(obj), OBJ(obj)))
			|| !jsc->getProperty
			||  jsc->getProperty == JS_PropertyStub
			|| !jsc->getProperty(CTX(obj), OBJ(obj), vid, &v))
		if (nt_value_is_number(idx)
				? !JS_GetElement(CTX(obj), OBJ(obj), nt_value_to_long(idx), &v)
				: !JS_GetPropertyById(CTX(obj), OBJ(obj), vid, &v))
			if ((isexc = JS_IsExceptionPending(CTX(obj))))
				JS_GetPendingException(CTX(obj), &v);
	return get_instance(obj, v, isexc);
}

static ntValue         *sm_value_set              (ntValue *obj, const ntValue *idx, const ntValue *value, ntPropAttr attrs) {
	jsint  flags  = (attrs & ntPropAttrDontEnum)   ? 0 : JSPROP_ENUMERATE;
		   flags |= (attrs & ntPropAttrReadOnly)   ? JSPROP_READONLY  : 0;
		   flags |= (attrs & ntPropAttrDontDelete) ? JSPROP_PERMANENT : 0;

	jsid vid;
	JS_ValueToId(CTX(idx), VAL(idx), &vid);

	jsval v = VAL(value);
	JSClass* jsc = NULL;
	if (!(jsc = JS_GetClass(CTX(obj), OBJ(obj)))
			|| !jsc->setProperty
			||  jsc->setProperty == JS_StrictPropertyStub
			|| !jsc->setProperty(CTX(obj), OBJ(obj), vid, false, &v)) {
		if (nt_value_is_number(idx)
					? !JS_SetElement(CTX(obj), OBJ(obj), nt_value_to_long(idx), &v)
					: !JS_SetPropertyById(CTX(obj), OBJ(obj), vid, &v)) {
			if (JS_IsExceptionPending(CTX(obj)) && JS_GetPendingException(CTX(obj), &v))
				return get_instance(obj, v, true);
			return sm_value_new_bool(obj, false);
		}
	}
	return sm_value_new_bool(obj, true);
}

static ntValue         *sm_value_enumerate        (const ntValue *obj) {
	jsint i;

	ntValue *array = NULL;
	ntValue *proto = nt_value_get_utf8(obj, "prototype");
	if (!nt_value_is_undefined(proto))
		array = nt_value_enumerate(proto);
	nt_value_decref(proto);

	JSIdArray* na = JS_Enumerate(CTX(obj), OBJ(obj));
	if (!na) return array;

	if (!array) array = nt_value_new_array(obj, NULL);
	for (i=0 ; i < na->length ; i++) {
		jsval str;
		if (!JS_IdToValue(CTX(obj), na->vector[i], &str)) break;
		ntValue *v = get_instance(obj, str, false);
		if (!v) break;
		nt_value_new_array_builder(array, v);
	}

	JS_DestroyIdArray(CTX(obj), na);
	return array;
}

static ntValue         *sm_value_call             (ntValue *func, ntValue *ths, ntValue *args) {
	// Convert to jsval array
	ntValue *length = nt_value_get_utf8(args, "length");
	long i, len = nt_value_to_long(length);
	nt_value_decref(length);

	jsval *argv = calloc(len, sizeof(jsval));
	for (i=0 ; i < len ; i++) {
		ntValue *item = nt_value_get_index(args, i);
		argv[i] = VAL(item);
		nt_value_decref(item);
	}

	// Call the function
	jsval rval;
	bool  exception = false;
	if (nt_value_is_undefined(ths)) {
		JSObject *obj = JS_New(CTX(func), OBJ(func), len, argv);
		if (!obj && JS_IsExceptionPending(CTX(func)) && JS_GetPendingException(CTX(func), &rval))
			exception = true;
		else if (obj)
			rval = OBJECT_TO_JSVAL(obj);
		else {
			free(argv);
			return nt_value_new_undefined(func);
		}
	} else {
		if (!JS_CallFunctionValue(CTX(func), OBJ(ths), VAL(func), len, argv, &rval)) {
			if (JS_IsExceptionPending(CTX(func)) && JS_GetPendingException(CTX(func), &rval))
				exception = true;
			else {
				free(argv);
				return nt_value_new_undefined(func);
			}
		}
	}
	free(argv);

	return get_instance(func, rval, exception);
}

static ntValue         *sm_value_evaluate         (ntValue *ths, const ntValue *jscript, const ntValue *filename, unsigned int lineno) {
	size_t len = 0;
	const jschar *jschars = JS_GetStringCharsAndLength(CTX(jscript), JS_ValueToString(CTX(jscript), VAL(jscript)), &len);
	char *fnchars = nt_value_to_string_utf8(filename, NULL);

	jsval val;
	if (JS_EvaluateUCScript(CTX(ths), OBJ(ths), jschars, len, fnchars, lineno, &val) == JS_FALSE) {
		free(fnchars);
		if (JS_IsExceptionPending(CTX(ths)) && JS_GetPendingException(CTX(ths), &val))
			return get_instance(ths, val, true);
		return nt_value_new_undefined(ths);
	}
	free(fnchars);

	return get_instance(ths, val, false);
}

static ntValue* sm_engine_newg(void *engine, ntValue *global) {
	ntValue *self = malloc(sizeof(smValue));
	if (!self) return NULL;
	memset(self, 0, sizeof(smValue));

	JSContext *ctx = NULL;
	JSObject  *glb = NULL;
	if (global) {
		ctx = CTX(global);
		glb = JS_NewGlobalObject(ctx, &glbdef);
	} else {
		ctx = JS_NewContext((JSRuntime*) engine, 1024 * 1024);
		if (!ctx) {
			free(self);
			return NULL;
		}
		JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_XML | JSOPTION_UNROOTED_GLOBAL);
		JS_SetVersion(ctx, JSVERSION_LATEST);
		JS_SetErrorReporter(ctx, report_error);

		glb = JS_NewCompartmentAndGlobalObject(ctx, &glbdef, NULL);
	}
	if (!glb || !JS_InitStandardClasses(ctx, glb)) {
		free(self);
		JS_DestroyContext(ctx);
		return NULL;
	}

	CTX(self) = ctx;
	VAL(self) = OBJECT_TO_JSVAL(glb);

	ntPrivate *priv = nt_private_init();
	if (!priv || !JS_SetPrivate(ctx, glb, priv)) {
		free(self);
		JS_DestroyContext(ctx);
	}

	JSVAL_LOCK(ctx, VAL(self));
	return self;
}

static void sm_engine_free(void *engine) {
	JS_DestroyRuntime((JSRuntime*) engine);
	JS_ShutDown();
}

static void *sm_engine_init() {
	JS_SetCStringsAreUTF8();
	return JS_NewRuntime(1024 * 1024);
}

NATUS_ENGINE("SpiderMonkey", "JS_GetProperty", sm);
