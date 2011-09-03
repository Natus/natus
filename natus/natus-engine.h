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

#ifndef NATUS_ENGINE_H_
#define NATUS_ENGINE_H_
#include <natus.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NATUS_ENGINE_VERSION 1
#define NATUS_ENGINE_ natus_engine__
#define NATUS_ENGINE(name, symb, prfx) \
  natusEngineSpec NATUS_ENGINE_ = { \
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

#ifndef NATUS_ENGINE_TYPES_DEFINED
typedef void* natusEngCtx;
typedef void* natusEngVal;
#endif

typedef struct natusPrivate natusPrivate;

typedef enum {
  natusPropertyActionDelete    = 1,
  natusPropertyActionGet       = 1 << 1,
  natusPropertyActionSet       = 1 << 2,
  natusPropertyActionEnumerate = 1 << 3
} natusPropertyAction;

typedef enum {
  natusEngValFlagNone      = 0,
  natusEngValFlagException = 1,
  natusEngValFlagUnlock    = 1 << 1,
  natusEngValFlagFree      = 1 << 2
} natusEngValFlags;

typedef struct {
  unsigned int version;
  const char  *name;
  const char  *symbol;

  void           (*ctx_free)         (natusEngCtx ctx);
  void           (*val_unlock)       (natusEngCtx ctx, natusEngVal val);
  natusEngVal    (*val_duplicate)    (natusEngCtx ctx, natusEngVal val);
  void           (*val_free)         (natusEngVal val);

  natusEngVal    (*new_global)       (natusEngCtx ctx, natusEngVal val, natusPrivate *priv, natusEngCtx *newctx, natusEngValFlags *flags);
  natusEngVal    (*new_bool)         (const natusEngCtx ctx, bool b, natusEngValFlags *flags);
  natusEngVal    (*new_number)       (const natusEngCtx ctx, double n, natusEngValFlags *flags);
  natusEngVal    (*new_string_utf8)  (const natusEngCtx ctx, const char *str, size_t len, natusEngValFlags *flags);
  natusEngVal    (*new_string_utf16) (const natusEngCtx ctx, const natusChar *str, size_t len, natusEngValFlags *flags);
  natusEngVal    (*new_array)        (const natusEngCtx ctx, const natusEngVal *array, size_t len, natusEngValFlags *flags);
  natusEngVal    (*new_function)     (const natusEngCtx ctx, const char *name, natusPrivate *priv, natusEngValFlags *flags);
  natusEngVal    (*new_object)       (const natusEngCtx ctx, natusClass *cls, natusPrivate *priv, natusEngValFlags *flags);
  natusEngVal    (*new_null)         (const natusEngCtx ctx, natusEngValFlags *flags);
  natusEngVal    (*new_undefined)    (const natusEngCtx ctx, natusEngValFlags *flags);

  bool           (*to_bool)          (const natusEngCtx ctx, const natusEngVal val);
  double         (*to_double)        (const natusEngCtx ctx, const natusEngVal val);
  char          *(*to_string_utf8)   (const natusEngCtx ctx, const natusEngVal val, size_t *len);
  natusChar     *(*to_string_utf16)  (const natusEngCtx ctx, const natusEngVal val, size_t *len);

  natusEngVal    (*del)              (const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags);
  natusEngVal    (*get)              (const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags);
  natusEngVal    (*set)              (const natusEngCtx ctx, natusEngVal val, const natusEngVal id, const natusEngVal value, natusPropAttr attrs, natusEngValFlags *flags);
  natusEngVal    (*enumerate)        (const natusEngCtx ctx, natusEngVal val, natusEngValFlags *flags);

  natusEngVal    (*call)             (const natusEngCtx ctx, natusEngVal func, natusEngVal ths, natusEngVal args, natusEngValFlags *flags);
  natusEngVal    (*evaluate)         (const natusEngCtx ctx, natusEngVal ths, const natusEngVal jscript, const natusEngVal filename, unsigned int lineno, natusEngValFlags *flags);

  natusPrivate  *(*get_private)      (const natusEngCtx ctx, const natusEngVal val);
  natusEngVal    (*get_global)       (const natusEngCtx ctx, const natusEngVal val, natusEngValFlags *flags);
  natusValueType (*get_type)         (const natusEngCtx ctx, const natusEngVal val);
  bool           (*borrow_context)   (natusEngCtx ctx, natusEngVal val, void **context, void **value);
  bool           (*equal)            (const natusEngCtx ctx, const natusEngVal val1, const natusEngVal val2, bool strict);
} natusEngineSpec;

natusEngVal natus_handle_property(natusPropertyAction act, natusEngVal obj, const natusPrivate *priv, natusEngVal idx, natusEngVal val, natusEngValFlags *flags);
natusEngVal natus_handle_call    (natusEngVal obj, const natusPrivate *priv, natusEngVal ths, natusEngVal arg, natusEngValFlags *flags);
void natus_private_free(natusPrivate *priv);
bool natus_private_push(natusPrivate *self, void *priv, natusFreeFunction free);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* NATUS_ENGINE_H_ */
