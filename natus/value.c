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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
typedef void* ntEngCtx;
typedef void* ntEngVal;
#include "backend.h"

#define  _str(s) # s
#define __str(s) _str(s)

#define NATUS_PRIV_CLASS     "natus::Class"
#define NATUS_PRIV_FUNCTION  "natus::Function"
#define NATUS_PRIV_GLOBAL    "natus::Global"

#define callandmkval(n, t, c, f, ...) \
	ntEngValFlags _flags = ntEngValFlagMustFree; \
	ntEngVal _val = c->ctx->eng->spec->f(__VA_ARGS__, &_flags); \
	n = mkval(c, _val, _flags, t);

#define callandreturn(t, c, f, ...) \
	callandmkval(ntValue *_value, t, c, f, __VA_ARGS__); \
	return _value

typedef struct _ntEngine ntEngine;
struct _ntEngine {
	size_t        ref;
	ntEngine     *next;
	ntEngine     *prev;
	void         *dll;
	ntEngineSpec *spec;
};

typedef struct _ntEngineContext ntEngineContext;
struct _ntEngineContext {
	size_t    ref;
	ntEngine *eng;
	ntEngCtx  ctx;
};

struct _ntValue {
	size_t           ref;
	ntEngineContext *ctx;
	ntValueType      typ;
	ntEngVal         val;
	ntEngValFlags    flg;
};

static ntEngine *engines;

#define new0(type) ((type*) malloc0(sizeof(type)))
static void *malloc0(size_t size) {
	void *tmp = malloc(size);
	if (tmp)
		return memset(tmp, 0, size);
	return NULL;
}

static void engine_decref(ntEngine **head, ntEngine *eng) {
	if (!eng)
		return;
	if (eng->ref > 0)
		eng->ref--;
	if (eng->ref == 0) {
		dlclose(eng->dll);
		if (eng->prev)
			eng->prev->next = eng->next;
		else
			*head = eng->next;
		free(eng);
	}
}

static ntEngine *do_load_file (const char *filename, bool reqsym) {
	ntEngine *eng;
	void *dll;

	/* Open the file */
	dll = dlopen (filename, RTLD_LAZY | RTLD_LOCAL);
	if (!dll) {
		fprintf(stderr, "%s -- %s\n", filename, dlerror());
		return NULL;
	}

	/* See if this engine was already loaded */
	for (eng = engines ; eng ; eng = eng->next) {
		if (dll == eng->dll) {
			dlclose(dll);
	 		eng->ref++;
			return eng;
		}
	}

	/* Create a new engine */
	eng = new0(ntEngine);
	if (!eng) {
		dlclose(dll);
		return NULL;
	}

	/* Get the engine spec */
	eng->spec = (ntEngineSpec*) dlsym (dll, __str(NATUS_ENGINE_));
	if (!eng->spec || eng->spec->version != NATUS_ENGINE_VERSION) {
		dlclose (dll);
		free(eng);
		return NULL;
	}

	/* Do the symbol check */
	if (eng->spec->symbol && reqsym) {
		void *tmp = dlopen (NULL, 0);
		if (!tmp || !dlsym (tmp, eng->spec->symbol)) {
			if (tmp)
				dlclose (tmp);
			dlclose (dll);
			free(eng);
			return NULL;
		}
		dlclose (tmp);
	}

	/* Insert the engine */
	eng->ref++;
	eng->dll = dll;
	eng->next = engines;
	if (eng->next)
		eng->next->prev = eng;
	return engines = eng;
}

static ntEngine *do_load_dir (const char *dirname, bool reqsym) {
	ntEngine *res = NULL;
	DIR *dir = opendir (dirname);
	if (!dir)
		return NULL;

	struct dirent *ent = NULL;
	while ((ent = readdir (dir))) {
		size_t flen = strlen (ent->d_name);
		size_t slen = strlen (MODSUFFIX);

		if (!strcmp (".", ent->d_name) || !strcmp ("..", ent->d_name))
			continue;
		if (flen < slen || strcmp (ent->d_name + flen - slen, MODSUFFIX))
			continue;

		char *tmp = (char *) malloc (sizeof(char) * (flen + strlen (dirname) + 2));
		if (!tmp)
			continue;

		strcpy (tmp, dirname);
		strcat (tmp, "/");
		strcat (tmp, ent->d_name);

		if ((res = do_load_file (tmp, reqsym))) {
			free (tmp);
			break;
		}
		free (tmp);
	}

	closedir (dir);
	return res;
}

static ntValue *mkval(const ntValue *ctx, ntEngVal val, ntEngValFlags flags, ntValueType type) {
	if (!ctx || !val)
		return NULL;

	ntValue *self = new0(ntValue);
	if (!self) {
		ctx->ctx->eng->spec->val_unlock(ctx->ctx->ctx, val);
		ctx->ctx->eng->spec->val_free(val);
		return NULL;
	}

	self->ref++;
	self->ctx = ctx->ctx;
	self->ctx->ref++;
	self->val = val;
	self->flg = flags;
	self->typ = flags & ntEngValFlagException ? ntValueTypeUnknown : type;
	return self;
}

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
		/* Free the value */
		if (val->val && (val->flg & ntEngValFlagMustFree)) {
			val->ctx->eng->spec->val_unlock(val->ctx->ctx, val->val);
			val->ctx->eng->spec->val_free(val->val);
		}

		/* Decrement the context reference */
		val->ctx->ref = val->ctx->ref > 0 ? val->ctx->ref - 1 : 0;

		/* If there are no more context references, free it */
		if (val->ctx->ref == 0) {
			val->ctx->eng->spec->ctx_free(val->ctx->ctx);
			engine_decref(&engines, val->ctx->eng); /* decrement the engine ref */
			free(val->ctx);
		}
		free(val);
		return NULL;
	}
	return val;
}

ntValue *nt_value_new_global (const char *name_or_path) {
	ntValue *self = NULL;
	ntPrivate *priv = NULL;

	/* Create the value */
	self = new0(ntValue);
	if (!self)
		goto error;
	self->ref++;
	self->ctx = new0(ntEngineContext);
	if (!self->ctx)
		goto error;
	self->ctx->ref++;

	/* Load the engine */
	if (name_or_path) {
		self->ctx->eng = do_load_file (name_or_path, false);
		if (!self->ctx->eng) {
			char *tmp = NULL;
			if (asprintf(&tmp, "%s/%s%s", __str(ENGINEDIR), name_or_path, MODSUFFIX) >= 0) {
				self->ctx->eng = do_load_file (tmp, false);
				free (tmp);
			}
		}
	} else {
		self->ctx->eng = engines;
		if (!self->ctx->eng) {
			self->ctx->eng = do_load_dir (__str(ENGINEDIR), true);
			if (!self->ctx->eng)
				self->ctx->eng = do_load_dir (__str(ENGINEDIR), false);
		}
	}
	if (!self->ctx->eng)
		goto error;

	priv = nt_private_init();
	if (!priv || !nt_private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
		nt_private_free(priv);
		goto error;
	}

	self->flg = ntEngValFlagMustFree;
	if (!(self->val = self->ctx->eng->spec->new_global(NULL, NULL, priv, &self->ctx->ctx, &self->flg)))
		goto error;

	return self;

	error:
		if (self) {
			if (self->ctx) {
				if (self->ctx->eng)
					engine_decref(&engines, self->ctx->eng);
				free(self->ctx);
			}
			free(self);
		}
		return NULL;
}

ntValue *nt_value_new_global_shared (const ntValue *global) {
	ntValue *self = NULL;
	ntEngCtx  ctx = NULL;
	ntEngVal  val = NULL;
	ntEngValFlags flags = ntEngValFlagMustFree;

	ntPrivate *priv = nt_private_init();
	if (!priv)
		return NULL;

	val = global->ctx->eng->spec->new_global(global->ctx->ctx, global->val, priv, &ctx, &flags);
	self = mkval(global, val, flags, ntValueTypeObject);
	if (!self)
		return NULL;

	/* If a new context was created, wrap it */
	if (global->ctx->ctx != ctx) {
		self->ctx->ref--;
		self->ctx = new0(ntEngineContext);
		if (!self->ctx) {
			global->ctx->eng->spec->val_unlock(ctx, self->val);
			global->ctx->eng->spec->val_free(self->val);
			global->ctx->eng->spec->ctx_free(ctx);
			free(self);
			return NULL;
		}
		self->ctx->ref++;
		self->ctx->ctx = ctx;
		self->ctx->eng = global->ctx->eng;
		self->ctx->eng->ref++;
	}

	if (!nt_private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
		nt_value_decref(self);
		return NULL;
	}

	return self;
}

ntValue *nt_value_new_boolean (const ntValue *ctx, bool b) {
	if (!ctx)
		return NULL;
	callandreturn(ntValueTypeBoolean, ctx, new_bool, ctx->ctx->ctx, b);
}

ntValue *nt_value_new_number (const ntValue *ctx, double n) {
	if (!ctx)
		return NULL;
	callandreturn(ntValueTypeNumber, ctx, new_number, ctx->ctx->ctx, n);
}

ntValue *nt_value_new_string (const ntValue *ctx, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	ntValue *tmpv = nt_value_new_string_varg(ctx, fmt, ap);
	va_end(ap);
	return tmpv;
}

ntValue *nt_value_new_string_varg (const ntValue *ctx, const char *fmt, va_list arg) {
	char *tmp = NULL;
	if (vasprintf(&tmp, fmt, arg) < 0)
		return NULL;
	ntValue *tmpv = nt_value_new_string_utf8(ctx, tmp);
	free(tmp);
	return tmpv;
}

ntValue *nt_value_new_string_utf8 (const ntValue *ctx, const char *string) {
	if (!string)
		return NULL;
	return nt_value_new_string_utf8_length (ctx, string, strlen (string));
}

ntValue *nt_value_new_string_utf8_length (const ntValue *ctx, const char *string, size_t len) {
	if (!ctx || !string)
		return NULL;
	callandreturn(ntValueTypeString, ctx, new_string_utf8, ctx->ctx->ctx, string, len);
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
	callandreturn(ntValueTypeString, ctx, new_string_utf16, ctx->ctx->ctx, string, len);
}

ntValue *nt_value_new_array (const ntValue *ctx, const ntValue **array) {
	const ntValue *novals[1] = { NULL, };
	size_t count = 0;
	size_t i;

	if (!ctx)
		return NULL;
	if (!array)
		array = novals;

	for (count=0 ; array[count] ; count++)
		;

	ntEngVal *vals = (ntEngVal*) malloc(sizeof(ntEngVal) * (count + 1));
	if (!vals)
		return NULL;

	for (i=0 ; i < count ; i++)
		vals[i] = array[i]->val;

	callandmkval(ntValue *val, ntValueTypeUnknown, ctx, new_array, ctx->ctx->ctx, vals, count);
	free(vals);
	return val;
}

ntValue *nt_value_new_function (const ntValue *ctx, ntNativeFunction func, const char *name) {
	if (!ctx || !func)
		return NULL;

	ntPrivate *priv = nt_private_init ();
	if (!priv)
		goto error;
	if (!nt_private_set (priv, NATUS_PRIV_GLOBAL, nt_value_get_global(ctx), NULL))
		goto error;
	if (!nt_private_set (priv, NATUS_PRIV_FUNCTION, func, NULL))
		goto error;

	callandreturn(ntValueTypeFunction, ctx, new_function, ctx->ctx->ctx, name, priv);

	error:
		nt_private_free (priv);
		return NULL;
}

ntValue *nt_value_new_object (const ntValue *ctx, ntClass *cls) {
	if (!ctx)
		return NULL;

	ntPrivate *priv = nt_private_init ();
	if (!priv)
		goto error;
	if (cls && !nt_private_set (priv, NATUS_PRIV_GLOBAL, nt_value_get_global(ctx), NULL))
		goto error;
	if (cls && !nt_private_set (priv, NATUS_PRIV_CLASS, cls, (ntFreeFunction) cls->free))
		goto error;

	callandreturn(ntValueTypeObject, ctx, new_object, ctx->ctx->ctx, cls, priv);

	error:
		nt_private_free (priv);
		return NULL;
}

ntValue *nt_value_new_null (const ntValue *ctx) {
	if (!ctx)
		return NULL;

	callandreturn(ntValueTypeNull, ctx, new_null, ctx->ctx->ctx);
}

ntValue *nt_value_new_undefined (const ntValue *ctx) {
	if (!ctx)
		return NULL;
	callandreturn(ntValueTypeUndefined, ctx, new_undefined, ctx->ctx->ctx);
}

ntValue *nt_value_get_global (const ntValue *ctx) {
	if (!ctx)
		return NULL;

	ntValue *global = nt_value_get_private_name(ctx, NATUS_PRIV_GLOBAL);
	if (global)
		return global;

	ntEngVal glb = ctx->ctx->eng->spec->get_global(ctx->ctx->ctx, ctx->val);
	if (!glb)
		return NULL;

	ntPrivate *priv = ctx->ctx->eng->spec->get_private(ctx->ctx->ctx, glb);
	ctx->ctx->eng->spec->val_unlock(ctx->ctx->ctx, glb);
	ctx->ctx->eng->spec->val_free(glb);
	if (!priv)
		return NULL;

	return nt_private_get(priv, NATUS_PRIV_GLOBAL);
}

const char *nt_value_get_engine_name(const ntValue *ctx) {
	if (!ctx)
		return NULL;
	return ctx->ctx->eng->spec->name;
}

ntValueType nt_value_get_type (const ntValue *ctx) {
	if (!ctx)
		return ntValueTypeUndefined;
	if (ctx->typ == ntValueTypeUnknown)
		((ntValue*) ctx)->typ = ctx->ctx->eng->spec->get_type(ctx->ctx->ctx, ctx->val);
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
	return ctx->ctx->eng->spec->borrow_context(ctx->ctx->ctx, ctx->val, context, value);
}

bool nt_value_is_global (const ntValue *val) {
	if (!nt_value_is_object (val))
		return false;

	ntValue *global = nt_value_get_global(val);
	if (!global)
		return false;

	return val->ctx->eng->spec->equal(val->ctx->ctx, global->val, val->val, ntEqualityStrictnessIdentity);
}

bool nt_value_is_exception (const ntValue *val) {
	return !val || (val->flg & ntEngValFlagException);
}

bool nt_value_is_type (const ntValue *val, ntValueType types) {
	return nt_value_get_type (val) & types;
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

	return val->ctx->eng->spec->to_bool(val->ctx->ctx, val->val);
}

double nt_value_to_double (const ntValue *val) {
	if (!val)
		return 0;
	return val->ctx->eng->spec->to_double(val->ctx->ctx, val->val);
}

ntValue *nt_value_to_exception (ntValue *val) {
	if (!val)
		return NULL;
	val->flg |= ntEngValFlagException;
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
			char *tmp = val->ctx->eng->spec->to_string_utf8 (str->ctx->ctx, str->val, len);
			nt_value_decref(str);
			return tmp;
		}
		nt_value_decref(str);
	}

	return val->ctx->eng->spec->to_string_utf8 (val->ctx->ctx, val->val, len);
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
			ntChar *tmp = val->ctx->eng->spec->to_string_utf16 (str->ctx->ctx, str->val, len);
			nt_value_decref(str);
			return tmp;
		}
		nt_value_decref(str);
	}

	return val->ctx->eng->spec->to_string_utf16 (val->ctx->ctx, val->val, len);
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

	callandreturn(ntValueTypeUnknown, val, del, val->ctx->ctx, val->val, id->val);
}

ntValue *nt_value_del_utf8 (ntValue *val, const char *id) {
	ntValue *vid = nt_value_new_string_utf8 (val, id);
	if (!vid)
		return NULL;
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

	callandreturn(ntValueTypeUnknown, val, get, val->ctx->ctx, val->val, id->val);
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

	callandreturn(ntValueTypeUnknown, val, set, val->ctx->ctx, val->val, id->val, value->val, attrs);
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

	callandreturn(ntValueTypeArray, val, enumerate, val->ctx->ctx, val->val);
}

bool nt_value_set_private_name (ntValue *obj, const char *key, void *priv, ntFreeFunction free) {
	if (!nt_value_is_type (obj, ntValueTypeSupportsPrivate))
		return false;
	if (!key)
		return false;

	ntPrivate *prv = obj->ctx->eng->spec->get_private (obj->ctx->ctx, obj->val);
	return nt_private_set (prv, key, priv, free);
}

static void free_private_value(ntValue *pv) {
	if (!pv)
		return;

	/* If the refcount is 0 it means that we are in the process of teardown,
	 * and the value has already been unlocked because we are dismantling ctx.
	 * Thus, we only do unlock if we aren't in this teardown phase. */
	if (pv->ctx->ref > 0)
		pv->ctx->eng->spec->val_unlock(pv->ctx->ctx, pv->val);
	pv->ctx->eng->spec->val_free(pv->val);
	free(pv);
}

bool nt_value_set_private_name_value (ntValue *obj, const char *key, ntValue* priv) {
	ntEngVal v = NULL;
	ntValue *val = NULL;

	if (priv) {
		/* NOTE: So we have this circular problem where if we hide just a normal
		 *       ntValue* it keeps a reference to the ctx, but the hidden value
		 *       won't be free'd until the ctx is destroyed, but that won't happen
		 *       because we have a reference.
		 *
		 *       So the way around this is that we don't store the normal ntValue,
		 *       but instead store a special one without a refcount on the ctx.
		 *       This means that ctx will be properly freed. */
		v = priv->ctx->eng->spec->val_duplicate(priv->ctx->ctx, priv->val);
		if (!v)
			return false;

		val = mkval(priv, v, priv->flg, priv->typ);
		if (!val)
			return false;
		val->ctx->ref--; /* Don't keep a copy of this reference around */
	}

	if (!nt_value_set_private_name(obj, key, val, (ntFreeFunction) free_private_value)) {
		free_private_value(val);
		return false;
	}
	return true;
}

void* nt_value_get_private_name (const ntValue *obj, const char *key) {
	if (!nt_value_is_type (obj, ntValueTypeSupportsPrivate))
		return NULL;
	if (!key)
		return NULL;
	ntPrivate *prv = obj->ctx->eng->spec->get_private (obj->ctx->ctx, obj->val);
	return nt_private_get (prv, key);
}

ntValue *nt_value_get_private_name_value (const ntValue *obj, const char *key) {
	/* See note in nt_value_set_private_name_value() */
	ntValue *val = nt_value_get_private_name (obj, key);
	if (!val)
		return NULL;
	return mkval(val,
			     val->ctx->eng->spec->val_duplicate(val->ctx->ctx, val->val),
			     val->flg,
			     val->typ);
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
	if (nt_value_is_array (stck))
		nt_value_decref (nt_value_set_index (stck, nt_value_as_long (nt_value_get_utf8 (stck, "length")), filename));

	callandmkval(ntValue *rslt,
			     ntValueTypeUnknown,
			     ths, evaluate,
			     ths->ctx->ctx,
			     ths->val,
				 jscript
				   ? jscript->val
				   : (javascript ? javascript->val : NULL),
				 filename
				   ? filename->val
				   : NULL,
				 lineno);
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
		callandmkval(res, ntValueTypeUnknown, func, call, func->ctx->ctx, func->val, ths->val, args->val);
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

bool nt_value_equals (const ntValue *val1, const ntValue *val2) {
	if (!val1 && !val2)
		return true;
	if (!val1 && nt_value_is_undefined (val2))
		return true;
	if (!val2 && nt_value_is_undefined (val1))
		return true;
	if (!val1 || !val2)
		return false;
	return val1->ctx->eng->spec->equal(val1->ctx->ctx, val1->val, val1->val, false);
}

bool nt_value_equals_strict (const ntValue *val1, const ntValue *val2) {
	if (!val1 && !val2)
		return true;
	if (!val1 && nt_value_is_undefined (val2))
		return true;
	if (!val2 && nt_value_is_undefined (val1))
		return true;
	if (!val1 || !val2)
		return false;
	return val1->ctx->eng->spec->equal(val1->ctx->ctx, val1->val, val1->val, true);
}

/**** ENGINE FUNCTIONS ****/

static ntEngVal return_ownership(ntValue *val, ntEngValFlags *flags) {
	ntEngVal ret = NULL;
	if (!flags)
		return NULL;
	*flags = ntEngValFlagNone;
	if (nt_value_is_exception(val))
		*flags |= ntEngValFlagException;
	if (!val)
		return NULL;

	/* If this value will free here, return ownership */
	ret = val->val;
	if (val->ref <= 1) {
		*flags |= ntEngValFlagMustFree;
		val->flg = val->flg & ~ntEngValFlagMustFree; /* Don't free this! */
	}
	nt_value_decref(val);

	return ret;
}

ntEngVal nt_value_handle_property(const ntPropertyAction act, ntEngVal obj, const ntPrivate *priv, ntEngVal idx, ntEngVal val, ntEngValFlags *flags) {
	ntValue *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	assert(glbl);

	/* Convert the arguments */
	ntValue *vobj = mkval(glbl, obj, ntEngValFlagMustFree, ntValueTypeUnknown);
	ntValue *vidx = mkval(glbl, act & ntPropertyActionEnumerate ? NULL : idx, ntEngValFlagMustFree, ntValueTypeUnknown);
	ntValue *vval = mkval(glbl, act & ntPropertyActionSet       ? val : NULL, ntEngValFlagMustFree, ntValueTypeUnknown);
	ntValue *rslt = NULL;

	/* Do the call */
	ntClass *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
	if (clss && vobj && (vidx || act & ntPropertyActionEnumerate) && (vval || act & ~ntPropertyActionSet)) {
		switch (act) {
			case ntPropertyActionDelete:
				rslt = clss->del(clss, vobj, vidx);
				break;
			case ntPropertyActionGet:
				rslt = clss->get(clss, vobj, vidx);
				break;
			case ntPropertyActionSet:
				rslt = clss->set(clss, vobj, vidx, vval);
				break;
			case ntPropertyActionEnumerate:
				rslt = clss->enumerate(clss, vobj);
				break;
			default:
				assert(false);
		}
	}
	nt_value_decref(vobj);
	nt_value_decref(vidx);
	nt_value_decref(vval);

	return return_ownership(rslt, flags);
}

ntEngVal nt_value_handle_call(ntEngVal obj, const ntPrivate *priv, ntEngVal ths, ntEngVal arg, ntEngValFlags *flags) {
	ntValue *glbl = nt_private_get(priv, NATUS_PRIV_GLOBAL);
	assert(glbl);

	/* Convert the arguments */
	ntValue *vobj = mkval(glbl, obj, ntEngValFlagMustFree, ntValueTypeUnknown);
	ntValue *vths = ths ? mkval(glbl, ths, ntEngValFlagMustFree, ntValueTypeUnknown) : nt_value_new_undefined(glbl);
	ntValue *varg = mkval(glbl, arg, ntEngValFlagMustFree, ntValueTypeUnknown);
	ntValue *rslt = NULL;
	if (vobj && vths && varg) {
		ntClass *clss = nt_private_get(priv, NATUS_PRIV_CLASS);
		ntNativeFunction func = nt_private_get(priv, NATUS_PRIV_FUNCTION);
		if (clss)
			rslt = clss->call(clss, vobj, vths, varg);
		else if (func)
			rslt = func(vobj, vths, varg);
	}

	/* Free the arguments */
	nt_value_decref(vobj);
	nt_value_decref(vths);
	nt_value_decref(varg);

	return return_ownership(rslt, flags);
}

void *nt_value_get_valuep(ntValue *v) {
	return v ? v->val : NULL;
}
