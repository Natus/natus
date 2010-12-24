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

#ifndef NATUSC_H_
#define NATUSC_H_

#ifndef I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#error Natus is not stable, go look elsewhere...
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ntValue ntValue;
typedef struct _ntEngine ntEngine;

/* Type: NativeFunction
 * Function type for calls made back into native space from javascript.
 *
 * Parameters:
 *     ths - The 'this' variable for the current execution.
 *           ths has a value of 'Undefined' when being run as a constructor.
 *     fnc - The object representing the function itself.
 *     arg - The argument vector.
 *
 * Returns:
 *     The return value, which may be an exception.
 */
typedef ntValue *(*ntNativeFunction)(ntValue *ths, ntValue *fnc, ntValue **arg);

/* Type: ntFreeFunction
 * Function type for calls made back to free a memory value allocated outside natus.
 *
 * Parameters:
 *     mem - The memory to free.
 */
typedef void (*ntFreeFunction)(void *mem);

typedef ntValue* (*ntRequireFunction)(ntValue* module, const char* name, const char* reldir, const char** path, void* misc);

typedef enum {
	ntPropAttrNone       = 0,
	ntPropAttrReadOnly   = 1 << 0,
	ntPropAttrDontEnum   = 1 << 1,
	ntPropAttrDontDelete = 1 << 2,
	ntPropAttrConstant   = ntPropAttrReadOnly | ntPropAttrDontDelete,
	ntPropAttrProtected  = ntPropAttrConstant | ntPropAttrDontEnum
} ntPropAttrs;

typedef struct _ntClass ntClass;
struct _ntClass {
	ntValue *(*del_prop) (ntValue *obj, const char *name);
	ntValue *(*del_item) (ntValue *obj, long idx);
	ntValue *(*get_prop) (ntValue *obj, const char *name);
	ntValue *(*get_item) (ntValue *obj, long idx);
	ntValue *(*set_prop) (ntValue *obj, const char *name, ntValue *value);
	ntValue *(*set_item) (ntValue *obj, long idx, ntValue *value);
	ntValue *(*enumerate)(ntValue *obj);
	ntValue *(*call)     (ntValue *obj, const ntValue* const* args);
	ntValue *(*call_new) (ntValue *obj, const ntValue* const* args);
	void     (*free)     (ntClass *cls);
};

ntEngine         *nt_engine_init(const char *name_or_path);
char             *nt_engine_name(ntEngine *engine);
ntValue          *nt_engine_new_global(ntEngine *engine, const char *config);
void              nt_engine_free(ntEngine *engine);

void              nt_free(ntValue *val);
ntValue          *nt_new_bool(const ntValue *ctx, bool b);
ntValue          *nt_new_number(const ntValue *ctx, double n);
ntValue          *nt_new_string(const ntValue *ctx, const char *string);
ntValue          *nt_new_array(const ntValue *ctx, ntValue **array);
ntValue          *nt_new_function(const ntValue *ctx, ntNativeFunction func);
ntValue          *nt_new_object(const ntValue *ctx, ntClass* cls);
ntValue          *nt_new_null(const ntValue *ctx);
ntValue          *nt_new_undefined(const ntValue *ctx);
ntValue          *nt_new_exception(const ntValue *ctx, const char *type, const char *message);
ntValue          *nt_new_exception_code(const ntValue *ctx, const char *type, const char *message, long code);
ntValue          *nt_new_exception_errno(const ntValue *ctx, int errorno);
ntValue          *nt_get_global(const ntValue *ctx);
void              nt_get_context(const ntValue *ctx, void **context, void **value);
ntValue          *nt_check_arguments(const ntValue *ctx, ntValue **arg, const char *fmt);

bool              nt_is_global(const ntValue *val);
bool              nt_is_exception(const ntValue *val);
bool              nt_is_oom(const ntValue *val);
bool              nt_is_array(const ntValue *val);
bool              nt_is_bool(const ntValue *val);
bool              nt_is_function(const ntValue *val);
bool              nt_is_null(const ntValue *val);
bool              nt_is_number(const ntValue *val);
bool              nt_is_object(const ntValue *val);
bool              nt_is_string(const ntValue *val);
bool              nt_is_undefined(const ntValue *val);

bool              nt_to_bool(const ntValue *val);
double            nt_to_double(const ntValue *val);
void              nt_to_exception(ntValue *val);
int               nt_to_int(const ntValue *val);
long              nt_to_long(const ntValue *val);
char             *nt_to_string(const ntValue *val);
char            **nt_to_string_vector(const ntValue *val);

bool              nt_del_property(ntValue *val, const char *name);
bool              nt_del_item(ntValue *val, long idx);
ntValue          *nt_get_property(const ntValue *val, const char *name);
ntValue          *nt_get_item(const ntValue *val, long idx);
bool              nt_has_property(const ntValue *val, const char *name);
bool              nt_has_item(const ntValue *val, long idx);
bool              nt_set_property(ntValue *val, const char *name, ntValue *value);
bool              nt_set_property_attr(ntValue *val, const char *name, ntValue *value, ntPropAttrs attrs);
bool              nt_set_property_full(ntValue *val, const char *name, ntValue *value, ntPropAttrs attrs, bool makeParents);
bool              nt_set_item(ntValue *val, long idx, ntValue *value);
char            **nt_enumerate(const ntValue *val);

long              nt_array_length(const ntValue *array);
long              nt_array_push(ntValue *array, ntValue *value);
ntValue          *nt_array_pop(ntValue *array);

bool              nt_set_private(ntValue *val, const char *key, void *priv);
bool              nt_set_private_full(ntValue *val, const char *key, void *priv, ntFreeFunction free);
void*             nt_get_private(const ntValue *val, const char *key);

ntValue          *nt_evaluate(ntValue *ths, const char *jscript, const char *filename, unsigned int lineno);
ntValue          *nt_evaluate_full(ntValue *ths, const char *jscript, const char *filename, unsigned int lineno, bool shift);
ntValue          *nt_call(ntValue *ths, ntValue *func, ntValue **args);
ntValue          *nt_call_name(ntValue *ths, const char *name, ntValue **args);
ntValue          *nt_new(ntValue *func, ntValue **args);
ntValue          *nt_new_name(ntValue *ths, const char *name, ntValue **args);

ntValue          *nt_from_json(ntValue *ctx, const char *json);
char             *nt_to_json(ntValue *obj);

ntValue          *nt_require(ntValue *ctx, const char *name, const char *reldir, const char **path);
void              nt_add_require_hook(ntValue* ctx, bool post, ntRequireFunction func, void* misc=NULL, ntFreeFunction free=NULL);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* NATUSC_H_ */

