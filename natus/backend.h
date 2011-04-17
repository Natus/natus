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

#ifndef ENGINEMOD_H_
#define ENGINEMOD_H_
#include "engine.h"
#include "value.h"
#include "private.h"

#ifndef I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#error Natus is not stable, go look elsewhere...
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NATUS_ENGINE_VERSION 1
#define NATUS_ENGINE_ natus_engine__
#define NATUS_ENGINE(name, symb, prfx) \
	ntEngineSpec NATUS_ENGINE_ = { \
		NATUS_ENGINE_VERSION, \
		name, \
		symb, \
		{ \
			prfx ## _engine_init, \
			prfx ## _engine_newg, \
			prfx ## _engine_free \
		}, \
		{ \
			prfx ## _value_private_get, \
			prfx ## _value_borrow_context, \
			prfx ## _value_get_global, \
			prfx ## _value_get_type, \
			prfx ## _value_free, \
			prfx ## _value_new_bool, \
			prfx ## _value_new_number, \
			prfx ## _value_new_string_utf8, \
			prfx ## _value_new_string_utf16, \
			prfx ## _value_new_array, \
			prfx ## _value_new_function, \
			prfx ## _value_new_object, \
			prfx ## _value_new_null, \
			prfx ## _value_new_undefined, \
			prfx ## _value_to_bool, \
			prfx ## _value_to_double, \
			prfx ## _value_to_string_utf8, \
			prfx ## _value_to_string_utf16, \
			prfx ## _value_del, \
			prfx ## _value_get, \
			prfx ## _value_set, \
			prfx ## _value_enumerate, \
			prfx ## _value_call, \
			prfx ## _value_evaluate \
		} \
	}
#define NATUS_PRIV_CLASS     "natus::Class"
#define NATUS_PRIV_FUNCTION  "natus::Function"
#define NATUS_PRIV_GLOBAL    "natus::Global"

typedef struct {
	void*    (*init)();
	ntValue *(*newg)(void *, ntValue *);
	void     (*free)(void *);
} ntEngineTable;

typedef struct {
	ntPrivate       *(*private_get)      (const ntValue *val);
	bool             (*borrow_context)   (const ntValue *ctx, void **context, void **value);
	ntValue         *(*get_global)       (const ntValue *val);
	ntValueType      (*get_type)         (const ntValue *val);
	void             (*free)             (ntValue *val);
	ntValue         *(*new_bool)         (const ntValue *ctx, bool b);
	ntValue         *(*new_number)       (const ntValue *ctx, double n);
	ntValue         *(*new_string_utf8)  (const ntValue *ctx, const char   *str, size_t len);
	ntValue         *(*new_string_utf16) (const ntValue *ctx, const ntChar *str, size_t len);
	ntValue         *(*new_array)        (const ntValue *ctx, const ntValue **array);
	ntValue         *(*new_function)     (const ntValue *ctx, ntPrivate *priv);
	ntValue         *(*new_object)       (const ntValue *ctx, ntClass *cls, ntPrivate *priv);
	ntValue         *(*new_null)         (const ntValue *ctx);
	ntValue         *(*new_undefined)    (const ntValue *ctx);
	bool             (*to_bool)          (const ntValue *val);
	double           (*to_double)        (const ntValue *val);
	char            *(*to_string_utf8)   (const ntValue *val, size_t *len);
	ntChar          *(*to_string_utf16)  (const ntValue *val, size_t *len);
	ntValue         *(*del)              (ntValue *obj, const ntValue *id);
	ntValue         *(*get)              (const ntValue *obj, const ntValue *id);
	ntValue         *(*set)              (ntValue *obj, const ntValue *id, const ntValue *value, ntPropAttr attrs);
	ntValue         *(*enumerate)        (const ntValue *obj);
	ntValue         *(*call)             (ntValue *func, ntValue *ths, ntValue *args);
	ntValue         *(*evaluate)         (ntValue *ths, const ntValue *jscript, const ntValue *filename, unsigned int lineno);
} ntValueTable;

typedef struct {
	unsigned int  version;
	const char   *name;
	const char   *symbol;
	ntEngineTable engine;
	ntValueTable  value;
} ntEngineSpec;

struct _ntEngine {
	size_t         ref;
	ntEngineSpec  *espec;
	void          *engine;
	void          *dll;
};

struct _ntValue {
	size_t         ref;
	ntEngine      *eng;
	ntValueType    typ;
};

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* ENGINEMOD_H_ */
