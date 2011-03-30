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

#ifndef VALUE_H_
#define VALUE_H_
#include "types.h"

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
typedef ntValue *(*ntNativeFunction)(ntValue *fnc, ntValue *ths, ntValue *arg);

typedef enum {
	_ntValueTypeUnknown  = 0, // Internal use only
	ntValueTypeArray     = 1 << 0,
	ntValueTypeBool      = 1 << 1,
	ntValueTypeFunction  = 1 << 2,
	ntValueTypeNull      = 1 << 3,
	ntValueTypeNumber    = 1 << 4,
	ntValueTypeObject    = 1 << 5,
	ntValueTypeString    = 1 << 6,
	ntValueTypeUndefined = 1 << 7,
	ntValueTypeSimple    = ntValueTypeBool | ntValueTypeNull | ntValueTypeNumber | ntValueTypeString | ntValueTypeUndefined,
	ntValueTypeComplex   = ntValueTypeArray | ntValueTypeFunction | ntValueTypeObject,
} ntValueType;

typedef enum {
	ntPropAttrNone       = 0,
	ntPropAttrReadOnly   = 1 << 0,
	ntPropAttrDontEnum   = 1 << 1,
	ntPropAttrDontDelete = 1 << 2,
	ntPropAttrConstant   = ntPropAttrReadOnly | ntPropAttrDontDelete,
	ntPropAttrProtected  = ntPropAttrConstant | ntPropAttrDontEnum
} ntPropAttr;

typedef struct _ntClass ntClass;
struct _ntClass {
	ntValue *(*del)      (ntClass *cls, ntValue *obj, const ntValue *prop);
	ntValue *(*get)      (ntClass *cls, ntValue *obj, const ntValue *prop);
	ntValue *(*set)      (ntClass *cls, ntValue *obj, const ntValue *prop, ntValue *value);
	ntValue *(*enumerate)(ntClass *cls, ntValue *obj);
	ntValue *(*call)     (ntClass *cls, ntValue *obj, ntValue *ths, ntValue *args);
	void     (*free)     (ntClass *cls);
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ntValue          *nt_value_incref                 (ntValue *val);
ntValue          *nt_value_decref                 (ntValue *val);

ntValue          *nt_value_new_bool               (const ntValue *ctx, bool b);
ntValue          *nt_value_new_number             (const ntValue *ctx, double n);
ntValue          *nt_value_new_string_utf8        (const ntValue *ctx, const   char *string);
ntValue          *nt_value_new_string_utf8_length (const ntValue *ctx, const   char *string, size_t len);
ntValue          *nt_value_new_string_utf16       (const ntValue *ctx, const ntChar *string);
ntValue          *nt_value_new_string_utf16_length(const ntValue *ctx, const ntChar *string, size_t len);
ntValue          *nt_value_new_array              (const ntValue *ctx, const ntValue **array);
ntValue          *nt_value_new_array_builder      (ntValue *array, ntValue *item);
ntValue          *nt_value_new_function           (const ntValue *ctx, ntNativeFunction func);
ntValue          *nt_value_new_object             (const ntValue *ctx, ntClass* cls);
ntValue          *nt_value_new_null               (const ntValue *ctx);
ntValue          *nt_value_new_undefined          (const ntValue *ctx);
ntValue          *nt_value_get_global             (const ntValue *ctx);
ntEngine         *nt_value_get_engine             (const ntValue *ctx);
ntValueType       nt_value_get_type               (const ntValue *ctx);
const char       *nt_value_get_type_name          (const ntValue *ctx);
bool              nt_value_borrow_context         (const ntValue *ctx, void **context, void **value);

bool              nt_value_is_global              (const ntValue *val);
bool              nt_value_is_exception           (const ntValue *val);
bool              nt_value_is_oom                 (const ntValue *val);
bool              nt_value_is_type                (const ntValue *val, ntValueType types);
bool              nt_value_is_array               (const ntValue *val);
bool              nt_value_is_bool                (const ntValue *val);
bool              nt_value_is_function            (const ntValue *val);
bool              nt_value_is_null                (const ntValue *val);
bool              nt_value_is_number              (const ntValue *val);
bool              nt_value_is_object              (const ntValue *val);
bool              nt_value_is_string              (const ntValue *val);
bool              nt_value_is_undefined           (const ntValue *val);

ntValue          *nt_value_to_exception           (ntValue *val);
bool              nt_value_to_bool                (const ntValue *val);
double            nt_value_to_double              (const ntValue *val);
long              nt_value_to_long                (const ntValue *val);
char             *nt_value_to_string_utf8         (const ntValue *val, size_t *len);
ntChar           *nt_value_to_string_utf16        (const ntValue *val, size_t *len);
bool              nt_value_as_bool                (ntValue *val);
double            nt_value_as_double              (ntValue *val);
long              nt_value_as_long                (ntValue *val);
char             *nt_value_as_string_utf8         (ntValue *val, size_t *len);
ntChar           *nt_value_as_string_utf16        (ntValue *val, size_t *len);

ntValue          *nt_value_del                    (      ntValue *obj, const ntValue *id);
ntValue          *nt_value_del_utf8               (      ntValue *obj, const    char *id);
ntValue          *nt_value_del_index              (      ntValue *obj, const   size_t id);
ntValue          *nt_value_del_recursive_utf8     (      ntValue *obj, const    char *id);
ntValue          *nt_value_get                    (const ntValue *obj, const ntValue *id);
ntValue          *nt_value_get_utf8               (const ntValue *obj, const    char *id);
ntValue          *nt_value_get_index              (const ntValue *obj, const   size_t id);
ntValue          *nt_value_get_recursive_utf8     (      ntValue *obj, const    char *id);
ntValue          *nt_value_set                    (      ntValue *obj, const ntValue *id, const ntValue *value, ntPropAttr attrs);
ntValue          *nt_value_set_utf8               (      ntValue *obj, const    char *id, const ntValue *value, ntPropAttr attrs);
ntValue          *nt_value_set_index              (      ntValue *obj, const   size_t id, const ntValue *value);
ntValue          *nt_value_set_recursive_utf8     (      ntValue *obj, const    char *id, const ntValue *value, ntPropAttr attrs, bool mkpath);
ntValue          *nt_value_enumerate              (const ntValue *obj);

bool              nt_value_private_set            (      ntValue *obj, const char *key, void *priv, ntFreeFunction free);
bool              nt_value_private_set_value      (      ntValue *obj, const char *key, ntValue* priv);
void             *nt_value_private_get            (const ntValue *obj, const char *key);
ntValue          *nt_value_private_get_value      (const ntValue *obj, const char *key);

ntValue          *nt_value_evaluate               (      ntValue *ths, const ntValue *javascript, const ntValue *filename, unsigned int lineno);
ntValue          *nt_value_evaluate_utf8          (      ntValue *ths, const    char *javascript, const    char *filename, unsigned int lineno);

ntValue          *nt_value_call                   (ntValue *func, ntValue *ths, ntValue *args);
ntValue          *nt_value_call_utf8              (ntValue *ths, const char *name, ntValue *args);
ntValue          *nt_value_call_new               (ntValue *func, ntValue *args);
ntValue          *nt_value_call_new_utf8          (ntValue *obj, const char *name, ntValue *args);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* VALUE_H_ */

