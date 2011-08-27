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
    prfx ## _ctx_free, \
    prfx ## _val_unlock, \
    prfx ## _val_duplicate, \
    prfx ## _val_free, \
    prfx ## _new_global, \
    prfx ## _new_bool, \
    prfx ## _new_number, \
    prfx ## _new_string_utf8, \
    prfx ## _new_string_utf16, \
    prfx ## _new_array, \
    prfx ## _new_function, \
    prfx ## _new_object, \
    prfx ## _new_null, \
    prfx ## _new_undefined, \
    prfx ## _to_bool, \
    prfx ## _to_double, \
    prfx ## _to_string_utf8, \
    prfx ## _to_string_utf16, \
    prfx ## _del, \
    prfx ## _get, \
    prfx ## _set, \
    prfx ## _enumerate, \
    prfx ## _call, \
    prfx ## _evaluate, \
    prfx ## _get_private, \
    prfx ## _get_global, \
    prfx ## _get_type, \
    prfx ## _borrow_context, \
    prfx ## _equal \
  }

typedef enum {
  ntEqualityStrictnessLoose,
  ntEqualityStrictnessStrict,
  ntEqualityStrictnessIdentity
} ntEqualityStrictness;

typedef enum {
  ntPropertyActionDelete    = 1,
  ntPropertyActionGet       = 1 << 1,
  ntPropertyActionSet       = 1 << 2,
  ntPropertyActionEnumerate = 1 << 3
} ntPropertyAction;

typedef enum {
  ntEngValFlagNone      = 0,
  ntEngValFlagException = 1,
  ntEngValFlagMustFree  = 1 << 1
} ntEngValFlags;

typedef struct {
  unsigned int version;
  const char *name;
  const char *symbol;

  void        (*ctx_free)         (ntEngCtx ctx);
  void        (*val_unlock)       (ntEngCtx ctx, ntEngVal val);
  ntEngVal    (*val_duplicate)    (ntEngCtx ctx, ntEngVal val);
  void        (*val_free)         (ntEngVal val);

  ntEngVal    (*new_global)       (ntEngCtx ctx, ntEngVal val, ntPrivate *priv, ntEngCtx *newctx, ntEngValFlags *flags);
  ntEngVal    (*new_bool)         (const ntEngCtx ctx, bool b, ntEngValFlags *flags);
  ntEngVal    (*new_number)       (const ntEngCtx ctx, double n, ntEngValFlags *flags);
  ntEngVal    (*new_string_utf8)  (const ntEngCtx ctx, const char *str, size_t len, ntEngValFlags *flags);
  ntEngVal    (*new_string_utf16) (const ntEngCtx ctx, const ntChar *str, size_t len, ntEngValFlags *flags);
  ntEngVal    (*new_array)        (const ntEngCtx ctx, const ntEngVal *array, size_t len, ntEngValFlags *flags);
  ntEngVal    (*new_function)     (const ntEngCtx ctx, const char *name, ntPrivate *priv, ntEngValFlags *flags);
  ntEngVal    (*new_object)       (const ntEngCtx ctx, ntClass *cls, ntPrivate *priv, ntEngValFlags *flags);
  ntEngVal    (*new_null)         (const ntEngCtx ctx, ntEngValFlags *flags);
  ntEngVal    (*new_undefined)    (const ntEngCtx ctx, ntEngValFlags *flags);

  bool        (*to_bool)          (const ntEngCtx ctx, const ntEngVal val);
  double      (*to_double)        (const ntEngCtx ctx, const ntEngVal val);
  char       *(*to_string_utf8)   (const ntEngCtx ctx, const ntEngVal val, size_t *len);
  ntChar     *(*to_string_utf16)  (const ntEngCtx ctx, const ntEngVal val, size_t *len);

  ntEngVal    (*del)              (const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags);
  ntEngVal    (*get)              (const ntEngCtx ctx, ntEngVal val, const ntEngVal id, ntEngValFlags *flags);
  ntEngVal    (*set)              (const ntEngCtx ctx, ntEngVal val, const ntEngVal id, const ntEngVal value, ntPropAttr attrs, ntEngValFlags *flags);
  ntEngVal    (*enumerate)        (const ntEngCtx ctx, ntEngVal val, ntEngValFlags *flags);

  ntEngVal    (*call)             (const ntEngCtx ctx, ntEngVal func, ntEngVal ths, ntEngVal args, ntEngValFlags *flags);
  ntEngVal    (*evaluate)         (const ntEngCtx ctx, ntEngVal ths, const ntEngVal jscript, const ntEngVal filename, unsigned int lineno, ntEngValFlags *flags);

  ntPrivate  *(*get_private)      (const ntEngCtx ctx, const ntEngVal val);
  ntEngVal    (*get_global)       (const ntEngCtx ctx, const ntEngVal val);
  ntValueType (*get_type)         (const ntEngCtx ctx, const ntEngVal val);
  bool        (*borrow_context)   (ntEngCtx ctx, ntEngVal val, void **context, void **value);
  bool        (*equal)            (const ntEngCtx ctx, const ntEngVal val1, const ntEngVal val2, ntEqualityStrictness strict);
} ntEngineSpec;

ntEngVal nt_value_handle_property(ntPropertyAction act, ntEngVal obj, const ntPrivate *priv, ntEngVal idx, ntEngVal val, ntEngValFlags *flags);
ntEngVal nt_value_handle_call    (ntEngVal obj, const ntPrivate *priv, ntEngVal ths, ntEngVal arg, ntEngValFlags *flags);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* ENGINEMOD_H_ */
