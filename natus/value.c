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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "backend.h"

ntValue *nt_value_incref (ntValue *val) {
	if (!val)
		return NULL;
	val->ref++;
	return val;
}

ntValue *nt_value_decref (ntValue *val) {
	if (!val)
		return NULL;
	val->ref = val->ref > 0 ? val->ref - 1 : 0;
	if (val->ref == 0) {
		val->eng->espec->value.free (val);
		return NULL;
	}
	return val;
}

ntValue *nt_value_new_boolean (const ntValue *ctx, bool b) {
	if (!ctx)
		return NULL;
	return ctx->eng->espec->value.new_bool (ctx, b);
}

ntValue *nt_value_new_number (const ntValue *ctx, double n) {
	if (!ctx)
		return NULL;
	return ctx->eng->espec->value.new_number (ctx, n);
}

ntValue *nt_value_new_string_utf8 (const ntValue *ctx, const char *string) {
	return nt_value_new_string_utf8_length (ctx, string, strlen (string));
}

ntValue *nt_value_new_string_utf8_length (const ntValue *ctx, const char *string, size_t len) {
	if (!ctx || !string)
		return NULL;
	return ctx->eng->espec->value.new_string_utf8 (ctx, string, len);
}

ntValue *nt_value_new_string_utf16 (const ntValue *ctx, const ntChar *string) {
	size_t len;
	for (len = 0; string && string[len] != L'\0' ; len++)
		;
	return nt_value_new_string_utf16_length (ctx, string, len);
}

ntValue *nt_value_new_string_utf16_length (const ntValue *ctx, const ntChar *string, size_t len) {
	if (!ctx || !string)
		return NULL;
	return ctx->eng->espec->value.new_string_utf16 (ctx, string, len);
}

ntValue *nt_value_new_array (const ntValue *ctx, const ntValue **array) {
	if (!ctx)
		return NULL;
	const ntValue *novals[1] = { NULL, };
	if (!array)
		array = novals;
	return ctx->eng->espec->value.new_array (ctx, array);
}

ntValue *nt_value_new_function (const ntValue *ctx, ntNativeFunction func, const char *name) {
	if (!ctx || !func)
		return NULL;

	ntPrivate *priv = nt_private_init ();
	if (!priv)
		goto error;
	if (!nt_private_set (priv, NATUS_PRIV_GLOBAL, ctx->eng->espec->value.get_global (ctx), NULL))
		goto error;
	if (!nt_private_set (priv, NATUS_PRIV_FUNCTION, func, NULL))
		goto error;
	return ctx->eng->espec->value.new_function (ctx, name, priv);

	error: nt_private_free (priv);
	return NULL;
}

ntValue *nt_value_new_object (const ntValue *ctx, ntClass *cls) {
	if (!ctx)
		return NULL;

	ntPrivate *priv = nt_private_init ();
	if (!priv)
		goto error;
	if (cls && !nt_private_set (priv, NATUS_PRIV_GLOBAL, ctx->eng->espec->value.get_global (ctx), NULL))
		goto error;
	if (cls && !nt_private_set (priv, NATUS_PRIV_CLASS, cls, (ntFreeFunction) cls->free))
		goto error;
	ntValue *rslt = ctx->eng->espec->value.new_object (ctx, cls, priv);
	if (rslt)
		return rslt;

	error: nt_private_free (priv);
	return NULL;
}

ntValue *nt_value_new_null (const ntValue *ctx) {
	if (!ctx)
		return NULL;
	return ctx->eng->espec->value.new_null (ctx);
}

ntValue *nt_value_new_undefined (const ntValue *ctx) {
	if (!ctx)
		return NULL;
	return ctx->eng->espec->value.new_undefined (ctx);
}

ntValue *nt_value_get_global (const ntValue *ctx) {
	if (!ctx)
		return NULL;
	return nt_value_incref (ctx->eng->espec->value.get_global (ctx));
}

ntEngine *nt_value_get_engine (const ntValue *ctx) {
	return nt_engine_incref (ctx->eng);
}

ntValueType nt_value_get_type (const ntValue *ctx) {
	if (!ctx)
		return ntValueTypeUndefined | ntValueTypeException;
	if ((ctx->typ & ~ntValueTypeException) == ntValueTypeUnknown)
		((ntValue*) ctx)->typ = ctx->eng->espec->value.get_type (ctx) | (ctx->typ & ntValueTypeException);
	return ctx->typ;
}

const char *nt_value_get_type_name (const ntValue *ctx) {
	switch (nt_value_get_type (ctx)) {
		case ntValueTypeArray:
			return "array";
		case ntValueTypeBoolean:
			return "boolean";
		case ntValueTypeFunction:
			return "function";
		case ntValueTypeNull:
			return "null";
		case ntValueTypeNumber:
			return "number";
		case ntValueTypeObject:
			return "object";
		case ntValueTypeString:
			return "string";
		case ntValueTypeUndefined:
			return "undefined";
		default:
			return "unknown";
	}
}

bool nt_value_borrow_context (const ntValue *ctx, void **context, void **value) {
	if (!ctx)
		return false;
	return ctx->eng->espec->value.borrow_context (ctx, context, value);
}

bool nt_value_is_global (const ntValue *val) {
	if (!val)
		return false;
	if (!nt_value_is_object (val))
		return false;
	return nt_value_get_private_name (val, NATUS_PRIV_GLOBAL) == val;
}

bool nt_value_is_exception (const ntValue *val) {
	return !val || (val->typ & ntValueTypeException);
}

bool nt_value_is_type (const ntValue *val, ntValueType types) {
	if ((types & ntValueTypeException) && !(val->typ & ntValueTypeException))
		return false;
	return (nt_value_get_type (val) & ~ntValueTypeException) & (types & ~ntValueTypeException);
}

bool nt_value_is_array (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeArray);
}

bool nt_value_is_boolean (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeBoolean);
}

bool nt_value_is_function (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeFunction);
}

bool nt_value_is_null (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeNull);
}

bool nt_value_is_number (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeNumber);
}

bool nt_value_is_object (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeObject);
}

bool nt_value_is_string (const ntValue *val) {
	return nt_value_is_type (val, ntValueTypeString);
}

bool nt_value_is_undefined (const ntValue *val) {
	return !val || nt_value_is_type (val, ntValueTypeUndefined);
}

bool nt_value_to_bool (const ntValue *val) {
	if (!val)
		return false;
	if (nt_value_is_exception (val))
		return false;
	return val->eng->espec->value.to_bool (val);
}

double nt_value_to_double (const ntValue *val) {
	if (!val)
		return 0;
	return val->eng->espec->value.to_double (val);
}

ntValue *nt_value_to_exception (ntValue *val) {
	if (!val)
		return NULL;
	val->typ |= ntValueTypeException;
	return val;
}

int nt_value_to_int (const ntValue *val) {
	if (!val)
		return 0;
	return (int) nt_value_to_double (val);
}

long nt_value_to_long (const ntValue *val) {
	if (!val)
		return 0;
	return (long) nt_value_to_double (val);
}

char *nt_value_to_string_utf8 (const ntValue *val, size_t *len) {
	if (!val)
		return 0;
	size_t intlen = 0;
	if (!len)
		len = &intlen;

	if (!nt_value_is_string(val)) {
		ntValue *str = nt_value_call_utf8((ntValue*) val, "toString", NULL);
		if (nt_value_is_string(str)) {
			char *tmp = val->eng->espec->value.to_string_utf8 (str, len);
			nt_value_decref(str);
			return tmp;
		}
		nt_value_decref(str);
	}

	return val->eng->espec->value.to_string_utf8 (val, len);
}

ntChar *nt_value_to_string_utf16 (const ntValue *val, size_t *len) {
	if (!val)
		return 0;
	size_t intlen = 0;
	if (!len)
		len = &intlen;

	if (!nt_value_is_string(val)) {
		ntValue *str = nt_value_call_utf8((ntValue*) val, "toString", NULL);
		if (nt_value_is_string(str)) {
			ntChar *tmp = val->eng->espec->value.to_string_utf16 (str, len);
			nt_value_decref(str);
			return tmp;
		}
		nt_value_decref(str);
	}

	return val->eng->espec->value.to_string_utf16 (val, len);
}

bool nt_value_as_bool (ntValue *val) {
	bool b = nt_value_to_bool (val);
	nt_value_decref (val);
	return b;
}

double nt_value_as_double (ntValue *val) {
	double d = nt_value_to_double (val);
	nt_value_decref (val);
	return d;
}

int nt_value_as_int (ntValue *val) {
	int i = nt_value_to_int (val);
	nt_value_decref (val);
	return i;
}

long nt_value_as_long (ntValue *val) {
	long l = nt_value_to_long (val);
	nt_value_decref (val);
	return l;
}

char *nt_value_as_string_utf8 (ntValue *val, size_t *len) {
	char *s = nt_value_to_string_utf8 (val, len);
	nt_value_decref (val);
	return s;
}

ntChar *nt_value_as_string_utf16 (ntValue *val, size_t *len) {
	ntChar *s = nt_value_to_string_utf16 (val, len);
	nt_value_decref (val);
	return s;
}

ntValue *nt_value_del (ntValue *val, const ntValue *id) {
	if (!nt_value_is_type (id, ntValueTypeNumber | ntValueTypeString))
		return NULL;

	// If the object is native, skip argument conversion and js overhead;
	//    call directly for increased speed
	ntClass *cls = nt_value_get_private_name (val, NATUS_PRIV_CLASS);
	if (cls && cls->del) {
		ntValue *rslt = cls->del (cls, val, id);
		if (!nt_value_is_undefined (rslt) || !nt_value_is_exception (rslt))
			return rslt;
		nt_value_decref (rslt);
	}

	return val->eng->espec->value.del (val, id);
}

ntValue *nt_value_del_utf8 (ntValue *val, const char *id) {
	ntValue *vid = nt_value_new_string_utf8 (val, id);
	if (!vid)
		return NULL;
	nt_value_incref (vid);
	ntValue *ret = nt_value_del (val, vid);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_del_index (ntValue *val, size_t id) {
	ntValue *vid = nt_value_new_number (val, id);
	if (!vid)
		return NULL;
	ntValue *ret = nt_value_del (val, vid);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_del_recursive_utf8 (ntValue *obj, const char *id) {
	if (!obj || !id)
		return NULL;
	if (!strcmp (id, ""))
		return obj;

	char *next = strchr (id, '.');
	if (next == NULL)
		return nt_value_del_utf8 (obj, id);
	char *base = strdup (id);
	if (!base)
		return NULL;
	base[next++ - id] = '\0';

	ntValue *basev = nt_value_get_utf8 (obj, base);
	ntValue *tmp = nt_value_del_recursive_utf8 (basev, next);
	nt_value_decref (basev);
	free (base);
	return tmp;
}

ntValue *nt_value_get (ntValue *val, const ntValue *id) {
	if (!nt_value_is_type (id, ntValueTypeNumber | ntValueTypeString))
		return NULL;

	// If the object is native, skip argument conversion and js overhead;
	//    call directly for increased speed
	ntClass *cls = nt_value_get_private_name (val, NATUS_PRIV_CLASS);
	if (cls && cls->get) {
		ntValue *rslt = cls->get (cls, val, id);
		if (!nt_value_is_undefined (rslt) || !nt_value_is_exception (rslt))
			return rslt;
		nt_value_decref (rslt);
	}

	return val->eng->espec->value.get (val, id);
}

ntValue *nt_value_get_utf8 (ntValue *val, const char *id) {
	ntValue *vid = nt_value_new_string_utf8 (val, id);
	if (!vid)
		return NULL;
	ntValue *ret = nt_value_get (val, vid);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_get_index (ntValue *val, size_t id) {
	ntValue *vid = nt_value_new_number (val, id);
	if (!vid)
		return NULL;
	ntValue *ret = nt_value_get (val, vid);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_get_recursive_utf8 (ntValue *obj, const char *id) {
	if (!obj || !id)
		return NULL;
	if (!strcmp (id, ""))
		return obj;

	char *next = strchr (id, '.');
	if (next == NULL)
		return nt_value_get_utf8 (obj, id);
	char *base = strdup (id);
	if (!base)
		return NULL;
	base[next++ - id] = '\0';

	ntValue *basev = nt_value_get_utf8 (obj, base);
	ntValue *tmp = nt_value_get_recursive_utf8 (basev, next);
	nt_value_decref (basev);
	free (base);
	return tmp;
}

ntValue *nt_value_set (ntValue *val, const ntValue *id, const ntValue *value, ntPropAttr attrs) {
	if (!nt_value_is_type (id, ntValueTypeNumber | ntValueTypeString))
		return NULL;
	if (!value)
		return NULL;

	// If the object is native, skip argument conversion and js overhead;
	//    call directly for increased speed
	ntClass *cls = nt_value_get_private_name (val, NATUS_PRIV_CLASS);
	if (cls && cls->set) {
		ntValue *rslt = cls->set (cls, val, id, value);
		if (!nt_value_is_undefined (rslt) || !nt_value_is_exception (rslt))
			return rslt;
		nt_value_decref (rslt);
	}

	return val->eng->espec->value.set (val, id, value, attrs);
}

ntValue *nt_value_set_utf8 (ntValue *val, const char *id, const ntValue *value, ntPropAttr attrs) {
	ntValue *vid = nt_value_new_string_utf8 (val, id);
	if (!vid)
		return NULL;
	ntValue *ret = nt_value_set (val, vid, value, attrs);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_set_index (ntValue *val, size_t id, const ntValue *value) {
	ntValue *vid = nt_value_new_number (val, id);
	if (!vid)
		return NULL;
	ntValue *ret = nt_value_set (val, vid, value, ntPropAttrNone);
	nt_value_decref (vid);
	return ret;
}

ntValue *nt_value_set_recursive_utf8 (ntValue *obj, const char *id, const ntValue *value, ntPropAttr attrs, bool mkpath) {
	if (!obj || !id)
		return NULL;
	if (!strcmp (id, ""))
		return obj;

	char *next = strchr (id, '.');
	if (next == NULL)
		return nt_value_set_utf8 (obj, id, value, attrs);
	char *base = strdup (id);
	if (!base)
		return NULL;
	base[next++ - id] = '\0';

	ntValue *step = nt_value_get_utf8 (obj, base);
	if (mkpath && (!step || nt_value_is_undefined (step))) {
		nt_value_decref (step);
		step = nt_value_new_object (obj, NULL);
		if (step) {
			ntValue *rslt = nt_value_set_utf8 (obj, base, step, attrs);
			if (nt_value_is_exception (rslt)) {
				nt_value_decref (step);
				free (base);
				return rslt;
			}
			nt_value_decref (rslt);
		}
	}
	free (base);

	ntValue *tmp = nt_value_set_recursive_utf8 (step, next, value, attrs, mkpath);
	nt_value_decref (step);
	return tmp;
}

ntValue *nt_value_enumerate (ntValue *val) {
	// If the object is native, skip argument conversion and js overhead;
	//    call directly for increased speed
	ntClass *cls = nt_value_get_private_name (val, NATUS_PRIV_CLASS);
	if (cls && cls->enumerate)
		return cls->enumerate (cls, val);

	return val->eng->espec->value.enumerate (val);
}

bool nt_value_set_private_name (ntValue *obj, const char *key, void *priv, ntFreeFunction free) {
	if (!nt_value_is_type (obj, ntValueTypeSupportsPrivate))
		return false;
	if (!key)
		return false;
	ntPrivate *prv = obj->eng->espec->value.private_get (obj);
	return nt_private_set (prv, key, priv, free);
}

bool nt_value_set_private_name_value (ntValue *obj, const char *key, ntValue* priv) {
	nt_value_incref (priv);
	if (!nt_value_set_private_name (obj, key, priv, (ntFreeFunction) nt_value_decref)) {
		nt_value_decref (priv);
		return false;
	}
	return true;
}

void* nt_value_get_private_name (const ntValue *obj, const char *key) {
	if (!nt_value_is_type (obj, ntValueTypeSupportsPrivate))
		return NULL;
	if (!key)
		return NULL;
	ntPrivate *prv = obj->eng->espec->value.private_get (obj);
	return nt_private_get (prv, key);
}

ntValue *nt_value_get_private_name_value (const ntValue *obj, const char *key) {
	return nt_value_incref (nt_value_get_private_name (obj, key));
}

ntValue *nt_value_evaluate (ntValue *ths, const ntValue *javascript, const ntValue *filename, unsigned int lineno) {
	if (!ths || !javascript)
		return NULL;
	ntValue *jscript = NULL;

	// Remove shebang line if present
	size_t len;
	ntChar *chars = nt_value_to_string_utf16 (javascript, &len);
	if (len > 2 && chars[0] == '#' && chars[1] == '!') {
		size_t i;
		for (i = 2; i < len ; i++) {
			if (chars[i] == '\n') {
				jscript = nt_value_new_string_utf16_length (ths, chars + i, len - i);
				break;
			}
		}
	}
	free (chars);

	// Add the file's directory to the require stack
	bool pushed = false;
	ntValue *glbl = nt_value_get_global (ths);
	ntValue *reqr = nt_value_get_utf8 (glbl, "require");
	ntValue *stck = nt_value_get_private_name_value (reqr, "natus.reqStack");
	nt_value_decref (glbl);
	if (nt_value_is_array (stck))
		nt_value_decref (nt_value_set_index (stck, nt_value_as_long (nt_value_get_utf8 (stck, "length")), filename));

	ntValue *rslt = ths->eng->espec->value.evaluate (ths, jscript ? jscript : javascript, filename, lineno);
	nt_value_decref (jscript);

	// Remove the directory from the stack
	if (pushed)
		nt_value_decref (nt_value_call_utf8 (stck, "pop", NULL));
	nt_value_decref (stck);
	nt_value_decref (reqr);
	return rslt;
}

ntValue *nt_value_evaluate_utf8 (ntValue *ths, const char *javascript, const char *filename, unsigned int lineno) {
	if (!ths || !javascript)
		return NULL;
	if (!(nt_value_get_type (ths) & (ntValueTypeArray | ntValueTypeFunction | ntValueTypeObject)))
		return NULL;

	ntValue *jscript = nt_value_new_string_utf8 (ths, javascript);
	if (!jscript)
		return NULL;

	ntValue *fname = NULL;
	if (filename) {
		fname = nt_value_new_string_utf8 (ths, filename);
		if (!fname) {
			nt_value_decref (jscript);
			return NULL;
		}
	}

	ntValue *ret = nt_value_evaluate (ths, jscript, fname, lineno);
	nt_value_decref (jscript);
	nt_value_decref (fname);
	return ret;
}

ntValue *nt_value_call (ntValue *func, ntValue *ths, ntValue *args) {
	ntValue *newargs = NULL;
	if (!args)
		args = newargs = nt_value_new_array (func, NULL);
	if (ths)
		nt_value_incref (ths);
	else
		ths = nt_value_new_undefined (func);

	// If the function is native, skip argument conversion and js overhead;
	//    call directly for increased speed
	ntNativeFunction fnc = nt_value_get_private_name (func, NATUS_PRIV_FUNCTION);
	ntClass *cls = nt_value_get_private_name (func, NATUS_PRIV_CLASS);
	ntValue *res = NULL;
	if (fnc || (cls && cls->call)) {
		if (fnc)
			res = fnc (func, ths, args);
		else
			res = cls->call (cls, func, ths, args);
	} else if (nt_value_is_function (func)) {
		res = func->eng->espec->value.call (func, ths, args);
	}

	nt_value_decref (ths);
	nt_value_decref (newargs);
	return res;
}

ntValue *nt_value_call_name (ntValue *ths, const ntValue *name, ntValue *args) {
	ntValue *func = nt_value_get (ths, name);
	if (!func)
		return NULL;

	ntValue *ret = nt_value_call (func, ths, args);
	nt_value_decref (func);
	return ret;
}

ntValue *nt_value_call_utf8 (ntValue *ths, const char *name, ntValue *args) {
	ntValue *func = nt_value_get_utf8 (ths, name);
	if (!func)
		return NULL;

	ntValue *ret = nt_value_call (func, ths, args);
	nt_value_decref (func);
	return ret;
}

ntValue *nt_value_call_new (ntValue *func, ntValue *args) {
	return nt_value_call (func, NULL, args);
}

ntValue *nt_value_call_new_utf8 (ntValue *obj, const char *name, ntValue *args) {
	ntValue *fnc = nt_value_get_utf8 (obj, name);
	ntValue *ret = nt_value_call (fnc, NULL, args);
	nt_value_decref (fnc);
	return ret;
}

bool nt_value_equals (ntValue *val1, ntValue *val2) {
	if (!val1 && !val2)
		return true;
	if (!val1 && nt_value_is_undefined (val2))
		return true;
	if (!val2 && nt_value_is_undefined (val1))
		return true;
	if (!val1 || !val2)
		return false;
	return val1->eng->espec->value.equal (val1, val2, false);
}

bool nt_value_equals_strict (ntValue *val1, ntValue *val2) {
	if (!val1 && !val2)
		return true;
	if (!val1 && nt_value_is_undefined (val2))
		return true;
	if (!val2 && nt_value_is_undefined (val1))
		return true;
	if (!val1 || !val2)
		return false;
	return val1->eng->espec->value.equal (val1, val2, true);
}
