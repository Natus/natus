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

#ifndef NATUS_H_
#define NATUS_H_

#ifndef I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#error Natus is not stable, go look elsewhere...
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define NATUS_CHECK_ARGUMENTS(arg, fmt) \
{ \
  natusValue *_exc = natus_ensure_arguments(arg, fmt); \
  if (natus_is_exception(_exc)) return _exc; \
  natus_decref(_exc); \
}

typedef struct natusValue natusValue;

#ifdef WIN32
typedef wchar_t natusChar;
#else
typedef uint16_t natusChar;
#endif

/* Type: natusFreeFunction
 * Function type for calls made back to free a memory value allocated outside natus.
 *
 * Parameters:
 *     mem - The memory to free.
 */
typedef void
(*natusFreeFunction)(void *mem);


/* Type: NativeFunction
 * Function type for calls made back into native space from javascript.
 *
 * Parameters:
 *     fnc - The object representing the function itself.
 *     ths - The 'this' variable for the current execution.
 *           ths has a value of 'Undefined' when being run as a constructor.
 *     arg - The argument vector.
 *
 * Returns:
 *     The return value, which may be an exception.
 */
typedef natusValue *
(*natusNativeFunction)(natusValue *fnc, natusValue *ths, natusValue *arg);

/* Allows you to filter calls to evaluate. Called twice: once before the
 * evaluation is performed and once after.
 *
 * In the first call javascript_or_return contains a reference to the
 * natusValue* containing the javascript to be evaluated, filename contains a
 * reference to the natusValue* containing the filename of the contents and
 * lineno contains a reference to the line number. You may overwrite any of
 * these values, but if you do, you must natus_decref() the original values.
 *
 * In the second call javascript_or_return contains a reference to the
 * natusValue* containing the return value from the evaluation, filename
 * contains a reference to the natusValue* containing the filename of the
 * contents evaluated and lineno is NULL. You may overwrite
 * javascript_or_return, but if you do, you must natus_decref() the original
 * value.
 *
 * Keep in mind that the evaluation may cause sub-evaluations, so if you
 * maintain state between calls you should wind and unwind like a stack.
 */
typedef void
(*natusEvaluateHook)(natusValue *ths, natusValue **javascript_or_return,
                     natusValue **filename, unsigned int *lineno, void *misc);

typedef enum {
  natusValueTypeUnknown = 0 << 0,
  natusValueTypeArray = 1 << 0,
  natusValueTypeBoolean = 1 << 1,
  natusValueTypeFunction = 1 << 2,
  natusValueTypeNull = 1 << 3,
  natusValueTypeNumber = 1 << 4,
  natusValueTypeObject = 1 << 5,
  natusValueTypeString = 1 << 6,
  natusValueTypeUndefined = 1 << 7,
  natusValueTypeSupportsPrivate = natusValueTypeFunction | natusValueTypeObject,
} natusValueType;

typedef enum {
  natusPropAttrNone = 0,
  natusPropAttrReadOnly = 1 << 0,
  natusPropAttrDontEnum = 1 << 1,
  natusPropAttrDontDelete = 1 << 2,
  natusPropAttrConstant = natusPropAttrReadOnly | natusPropAttrDontDelete,
  natusPropAttrProtected = natusPropAttrConstant | natusPropAttrDontEnum
} natusPropAttr;

typedef struct natusClass natusClass;
struct natusClass {

  natusValue *
  (*del)(natusClass *cls, natusValue *obj, const natusValue *prop);

  natusValue *
  (*get)(natusClass *cls, natusValue *obj, const natusValue *prop);

  natusValue *
  (*set)(natusClass *cls, natusValue *obj, const natusValue *prop, const natusValue *value);

  natusValue *
  (*enumerate)(natusClass *cls, natusValue *obj);

  natusValue *
  (*call)(natusClass *cls, natusValue *obj, natusValue *ths, natusValue *args);

  void
  (*free)(natusClass *cls);
};

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
natusValue *
natus_incref(natusValue *val);

void
natus_decref(natusValue *val);

natusValue *
natus_new_global(const char *name_or_path);

natusValue *
natus_new_global_shared(const natusValue *global);

natusValue *
natus_new_boolean(const natusValue *ctx, bool b);

natusValue *
natus_new_number(const natusValue *ctx, double n);

natusValue *
natus_new_string(const natusValue *ctx, const char *fmt, ...);

natusValue *
natus_new_string_varg(const natusValue *ctx, const char *fmt, va_list arg);

natusValue *
natus_new_string_utf8(const natusValue *ctx, const char *string);

natusValue *
natus_new_string_utf8_length(const natusValue *ctx, const char *string, size_t len);

natusValue *
natus_new_string_utf16(const natusValue *ctx, const natusChar *string);

natusValue *
natus_new_string_utf16_length(const natusValue *ctx, const natusChar *string, size_t len);

natusValue *
natus_new_array(const natusValue *ctx, ...);

natusValue *
natus_new_array_vector(const natusValue *ctx, const natusValue **array);

natusValue *
natus_new_array_varg(const natusValue *ctx, va_list ap);

natusValue *
natus_new_function(const natusValue *ctx, natusNativeFunction func, const char *name);

natusValue *
natus_new_object(const natusValue *ctx, natusClass* cls);

natusValue *
natus_new_null(const natusValue *ctx);

natusValue *
natus_new_undefined(const natusValue *ctx);

natusValue *
natus_get_global(const natusValue *ctx);

const char *
natus_get_engine_name(const natusValue *ctx);

natusValueType
natus_get_type(const natusValue *ctx);

const char *
natus_get_type_name(const natusValue *ctx);

bool
natus_borrow_context(const natusValue *ctx, void **context, void **value);

bool
natus_is_exception(const natusValue *val);

bool
natus_is_type(const natusValue *val, natusValueType types);

bool
natus_is_array(const natusValue *val);

bool
natus_is_boolean(const natusValue *val);

bool
natus_is_function(const natusValue *val);

bool
natus_is_null(const natusValue *val);

bool
natus_is_number(const natusValue *val);

bool
natus_is_object(const natusValue *val);

bool
natus_is_string(const natusValue *val);

bool
natus_is_undefined(const natusValue *val);

natusValue *
natus_to_exception(natusValue *val);

bool
natus_to_bool(const natusValue *val);

double
natus_to_double(const natusValue *val);

int
natus_to_int(const natusValue *val);

long
natus_to_long(const natusValue *val);

char *
natus_to_string_utf8(const natusValue *val, size_t *len);

natusChar *
natus_to_string_utf16(const natusValue *val, size_t *len);

bool
natus_as_bool(natusValue *val);

double
natus_as_double(natusValue *val);

int
natus_as_int(natusValue *val);

long
natus_as_long(natusValue *val);

char *
natus_as_string_utf8(natusValue *val, size_t *len);

natusChar *
natus_as_string_utf16(natusValue *val, size_t *len);

natusValue *
natus_del(natusValue *obj, const natusValue *id);

natusValue *
natus_del_utf8(natusValue *obj, const char *id);

natusValue *
natus_del_index(natusValue *obj, const size_t id);

natusValue *
natus_del_recursive_utf8(natusValue *obj, const char *id);

natusValue *
natus_get(natusValue *obj, const natusValue *id);

natusValue *
natus_get_utf8(natusValue *obj, const char *id);

natusValue *
natus_get_index(natusValue *obj, const size_t id);

natusValue *
natus_get_recursive_utf8(natusValue *obj, const char *id);

natusValue *
natus_set(natusValue *obj, const natusValue *id, const natusValue *value, natusPropAttr attrs);

natusValue *
natus_set_utf8(natusValue *obj, const char *id, const natusValue *value, natusPropAttr attrs);

natusValue *
natus_set_index(natusValue *obj, const size_t id, const natusValue *value);

natusValue *
natus_set_recursive_utf8(natusValue *obj, const char *id, const natusValue *value, natusPropAttr attrs, bool mkpath);

natusValue *
natus_enumerate(natusValue *obj);

natusValue *
natus_push(natusValue *array, natusValue *val);

natusValue *
natus_pop(natusValue *array);

#define natus_set_private(obj, type, priv, free) \
  natus_set_private_name(obj, # type, priv, free)

bool
natus_set_private_name(natusValue *obj, const char *key, void *priv, natusFreeFunction free);

bool
natus_set_private_name_value(natusValue *obj, const char *key, natusValue* priv);

#define natus_get_private(obj, type) \
  ((type) natus_get_private_name(obj, # type))

void *
natus_get_private_name(const natusValue *obj, const char *key);

natusValue *
natus_get_private_name_value(const natusValue *obj, const char *key);

natusValue *
natus_evaluate(natusValue *ths, natusValue *javascript, natusValue *filename, unsigned int lineno);

natusValue *
natus_evaluate_utf8(natusValue *ths, const char *javascript, const char *filename, unsigned int lineno);

natusValue *
natus_call(natusValue *func, natusValue *ths, ...);

natusValue *
natus_call_varg(natusValue *func, natusValue *ths, va_list ap);

natusValue *
natus_call_array(natusValue *func, natusValue *ths, natusValue *args);

natusValue *
natus_call_utf8(natusValue *ths, const char *name, ...);

natusValue *
natus_call_utf8_varg(natusValue *ths, const char *name, va_list ap);

natusValue *
natus_call_utf8_array(natusValue *ths, const char *name, natusValue *args);

natusValue *
natus_call_new(natusValue *func, ...);

natusValue *
natus_call_new_varg(natusValue *func, va_list ap);

natusValue *
natus_call_new_array(natusValue *func, natusValue *args);

natusValue *
natus_call_new_utf8(natusValue *obj, const char *name, ...);

natusValue *
natus_call_new_utf8_varg(natusValue *obj, const char *name, va_list ap);

natusValue *
natus_call_new_utf8_array(natusValue *obj, const char *name, natusValue *args);

bool
natus_equals(const natusValue *val1, const natusValue *val2);

bool
natus_equals_strict(const natusValue *val1, const natusValue *val2);

natusValue *
natus_throw_exception(const natusValue *ctx, const char *base,
                      const char *type, const char *format, ...);

natusValue *
natus_throw_exception_varg(const natusValue *ctx, const char *base,
                           const char *type, const char *format, va_list ap);

natusValue *
natus_throw_exception_code(const natusValue *ctx, const char *base,
                           const char *type, int code, const char *format,
                           ...);

natusValue *
natus_throw_exception_code_varg(const natusValue *ctx, const char *base,
                                const char *type, int code, const char *format,
                                va_list ap);

natusValue *
natus_throw_exception_errno(const natusValue *ctx, int errorno);

natusValue *
natus_ensure_arguments(natusValue *arg, const char *fmt);

natusValue *
natus_convert_arguments(natusValue *arg, const char *fmt, ...);

natusValue *
natus_convert_arguments_varg(natusValue *arg, const char *fmt, va_list ap);

bool
natus_evaluate_hook_add(natusValue *ctx, const char *name,
                        natusEvaluateHook hook, void *misc,
                        natusFreeFunction free);

bool
natus_evaluate_hook_del(natusValue *ctx, const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* NATUS_H_ */

