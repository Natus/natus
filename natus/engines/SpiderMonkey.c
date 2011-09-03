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

typedef JSContext* natusEngCtx;
typedef jsval* natusEngVal;
#define NATUS_ENGINE_TYPES_DEFINED
#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-engine.h>

static void
sm_val_unlock(natusEngCtx ctx, natusEngVal val);
static void
sm_val_free(natusEngVal val);
static natusValueType
sm_get_type(const natusEngCtx ctx, const natusEngVal val);

static natusEngVal
mkjsval(JSContext *ctx, jsval val)
{
  jsval *v = malloc(sizeof(jsval));
  if (!v)
    return NULL;
  *v = val;
  JSVAL_LOCK(ctx, *v);
  return v;
}

static void
obj_finalize(JSContext *ctx, JSObject *obj);

static natusPrivate *
get_private(JSContext *ctx, JSObject *obj)
{
  if (!ctx || !obj)
    return NULL;
  if (JS_IsArrayObject(ctx, obj))
    return NULL;

  if (JS_ObjectIsFunction(ctx, obj)) {
    jsval privval;
    if (!JS_GetReservedSlot(ctx, obj, 0, &privval) || !JSVAL_IS_OBJECT(privval))
      return NULL;
    obj = JSVAL_TO_OBJECT(privval);
  }

  JSClass *cls = JS_GetClass(ctx, obj);
  if (!cls || cls->finalize != obj_finalize)
    return NULL;

  return JS_GetPrivate(ctx, obj);
}

static JSBool
property_handler(JSContext *ctx, JSObject *object, jsid id, jsval *vp, natusPropertyAction act)
{
  natusPrivate *priv = get_private(ctx, object);
  if (!priv)
    return JS_FALSE;

  jsval key;
  if (act & ~natusPropertyActionEnumerate) {
    if (!JS_IdToValue(ctx, id, &key))
      return JS_FALSE;
    assert(JSVAL_IS_STRING(key) || JSVAL_IS_NUMBER(key));
  }

  natusEngValFlags flags = natusEngValFlagNone;
  natusEngVal obj = mkjsval(ctx, OBJECT_TO_JSVAL(object));
  natusEngVal idx = (act & ~natusPropertyActionEnumerate) ? mkjsval(ctx, key) : NULL;
  natusEngVal val = (act & natusPropertyActionSet) ? mkjsval(ctx, *vp) : NULL;
  natusEngVal res = natus_handle_property(act, obj, priv, idx, val, &flags);

  if (flags & natusEngValFlagException) {
    if (!res || JSVAL_IS_VOID(*res)) {
      if ((flags & natusEngValFlagUnlock) && res)
        sm_val_unlock(ctx, res);
      if ((flags & natusEngValFlagFree) && res)
        sm_val_free(res);
      return JS_TRUE;
    }

    JS_SetPendingException(ctx, *res);
    if (flags & natusEngValFlagUnlock)
      sm_val_unlock(ctx, res);
    if (flags & natusEngValFlagFree)
      sm_val_free(res);
    return JS_FALSE;
  }

  if (act == natusPropertyActionGet)
    *vp = *res;

  if (flags & natusEngValFlagUnlock)
    sm_val_unlock(ctx, res);
  if (flags & natusEngValFlagFree)
    sm_val_free(res);
  return JS_TRUE;
}

static inline JSBool
call_handler(JSContext *ctx, uintN argc, jsval *vp, bool constr)
{
  // Allocate our array of arguments
  JSObject *arga = JS_NewArrayObject(ctx, argc, JS_ARGV(ctx, vp));
  if (!arga)
    return JS_FALSE;

  // Get the private
  natusPrivate *priv = get_private(ctx, JSVAL_TO_OBJECT(JS_CALLEE(ctx, vp)));
  if (!priv)
    return JS_FALSE;

  // Do the call
  natusEngValFlags flags;
  natusEngVal obj = mkjsval(ctx, JS_CALLEE(ctx, vp));
  natusEngVal ths = mkjsval(ctx, constr ? JSVAL_VOID : JS_THIS(ctx, vp));
  natusEngVal arg = mkjsval(ctx, OBJECT_TO_JSVAL(arga));
  natusEngVal res = natus_handle_call(obj, priv, ths, arg, &flags);

  // Handle the results
  if (flags & natusEngValFlagException) {
    JS_SetPendingException(ctx, res ? *res : JSVAL_VOID);
    if ((flags & natusEngValFlagUnlock) && res)
      sm_val_unlock(ctx, res);
    if ((flags & natusEngValFlagFree) && res)
      sm_val_free(res);
    return JS_FALSE;
  }

  JS_SET_RVAL(ctx, vp, *res);
  if (flags & natusEngValFlagUnlock)
    sm_val_unlock(ctx, res);
  if (flags & natusEngValFlagFree)
    sm_val_free(res);
  return JS_TRUE;
}

static void
obj_finalize(JSContext *ctx, JSObject *obj)
{
  natus_private_free(get_private(ctx, obj));
}

static JSBool
obj_del(JSContext *ctx, JSObject *object, jsid id, jsval *vp)
{
  return property_handler(ctx, object, id, vp, natusPropertyActionDelete);
}

static JSBool
obj_get(JSContext *ctx, JSObject *object, jsid id, jsval *vp)
{
  return property_handler(ctx, object, id, vp, natusPropertyActionGet);
}

static JSBool
obj_set(JSContext *ctx, JSObject *object, jsid id, JSBool strict, jsval *vp)
{
  return property_handler(ctx, object, id, vp, natusPropertyActionSet);
}

static JSBool
obj_enum(JSContext *ctx, JSObject *object, JSIterateOp enum_op, jsval *statep, jsid *idp)
{
  jsuint len;
  jsval step;
  if (enum_op == JSENUMERATE_NEXT) {
    assert(statep && idp);
    // Get our length and the current step
    if (!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(*statep), &len))
      return JS_FALSE;
    if (!JS_GetProperty(ctx, JSVAL_TO_OBJECT(*statep), "step", &step))
      return JS_FALSE;

    // If we have finished our iteration, end
    if ((jsuint) JSVAL_TO_INT(step) >= len) {
      JSVAL_UNLOCK(ctx, *statep);
      *statep = JSVAL_NULL;
      return JS_TRUE;
    }

    // Get the item
    jsval item;
    if (!JS_GetElement(ctx, JSVAL_TO_OBJECT(*statep), JSVAL_TO_INT(step), &item))
      return JS_FALSE;
    JS_ValueToId(ctx, item, idp);

    // Increment to the next step
    step = INT_TO_JSVAL(JSVAL_TO_INT(step) + 1);
    if (!JS_SetProperty(ctx, JSVAL_TO_OBJECT(*statep), "step", &step))
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
  step = INT_TO_JSVAL(0);
  if (!property_handler(ctx, object, 0, &res, natusPropertyActionEnumerate)
      || sm_get_type(ctx, &res) != natusValueTypeArray
      || !JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(res), &len)
      || !JS_SetProperty(ctx, JSVAL_TO_OBJECT(res), "step", &step))
    return JS_FALSE;

  // Keep the jsval, but dispose of the wrapper
  *statep = res;
  JSVAL_LOCK(ctx, *statep);
  if (idp)
    *idp = INT_TO_JSVAL(len);
  return JS_TRUE;
}

static JSBool
obj_call(JSContext *ctx, uintN argc, jsval *vp)
{
  return call_handler(ctx, argc, vp, false);
}

static JSBool
obj_new(JSContext *ctx, uintN argc, jsval *vp)
{
  return call_handler(ctx, argc, vp, true);
}

static JSBool
fnc_call(JSContext *ctx, uintN argc, jsval *vp)
{
  return call_handler(ctx, argc, vp, JS_IsConstructing(ctx, vp));
}

static JSClass glbdef =
  { "GlobalObject", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static JSClass fncdef =
  { "NativeFunction", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static JSClass objdef =
  { "NativeObject", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, obj_finalize, };

static void
report_error(JSContext *cx, const char *message, JSErrorReport *report)
{
  fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename : "<no filename>", (unsigned int) report->lineno, message);
}

static void
sm_ctx_free(natusEngCtx ctx)
{
  JSRuntime *run = JS_GetRuntime(ctx);
  JS_GC(ctx);
  JS_DestroyContext(ctx);
  JS_DestroyRuntime(run);
}

static void
sm_val_unlock(natusEngCtx ctx, natusEngVal val)
{
  JSVAL_UNLOCK(ctx, *val);
}

static natusEngVal
sm_val_duplicate(natusEngCtx ctx, natusEngVal val)
{
  return mkjsval(ctx, *val);
}

static void
sm_val_free(natusEngVal val)
{
  free(val);
}

static natusEngVal
sm_new_global(natusEngCtx ctx, natusEngVal val, natusPrivate *priv, natusEngCtx *newctx, natusEngValFlags *flags)
{
  bool freectx = false;
  JSObject *glb = NULL;

  /* Reuse existing runtime and context */
  if (ctx && val) {
    *newctx = ctx;
    glb = JS_NewGlobalObject(ctx, &glbdef);

    /* Create a new runtime and context */
  } else {
    freectx = true;

    JSRuntime *run = JS_NewRuntime(1024 * 1024);
    if (!run)
      return NULL;

    *newctx = JS_NewContext(run, 1024 * 1024);
    if (!*newctx) {
      JS_DestroyRuntime(run);
      return NULL;
    }
    JS_SetOptions(*newctx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_XML | JSOPTION_UNROOTED_GLOBAL);
    JS_SetVersion(*newctx, JSVERSION_LATEST);
    JS_SetErrorReporter(*newctx, report_error);

    glb = JS_NewCompartmentAndGlobalObject(*newctx, &glbdef, NULL);
  }
  if (!glb || !JS_InitStandardClasses(*newctx, glb)) {
    if (freectx && *newctx) {
      JSRuntime *run = JS_GetRuntime(*newctx);
      JS_DestroyContext(*newctx);
      JS_DestroyRuntime(run);
    }
    return NULL;
  }

  if (!JS_SetPrivate(ctx, glb, priv)) {
    natus_private_free(priv);
    if (freectx && *newctx) {
      JSRuntime *run = JS_GetRuntime(*newctx);
      JS_DestroyContext(*newctx);
      JS_DestroyRuntime(run);
    }
    return NULL;
  }

  natusEngVal v = mkjsval(*newctx, OBJECT_TO_JSVAL(glb));
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

static natusEngVal
sm_new_bool(const natusEngCtx ctx, bool b, natusEngValFlags *flags)
{
  return mkjsval(ctx, BOOLEAN_TO_JSVAL(b));
}

static natusEngVal
sm_new_number(const natusEngCtx ctx, double n, natusEngValFlags *flags)
{
  jsval v = JSVAL_VOID;
  if (!JS_NewNumberValue(ctx, n, &v)) {
    v = JSVAL_VOID;
    if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
      *flags |= natusEngValFlagException;
  }
  return mkjsval(ctx, v);
}

static natusEngVal
sm_new_string_utf8(const natusEngCtx ctx, const char *str, size_t len, natusEngValFlags *flags)
{
  jsval v = JSVAL_VOID;
  JSString *s = JS_NewStringCopyN(ctx, str, len);
  if (s)
    v = STRING_TO_JSVAL(s);
  else {
    if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
      *flags |= natusEngValFlagException;
  }

  return mkjsval(ctx, STRING_TO_JSVAL(s));
}

static natusEngVal
sm_new_string_utf16(const natusEngCtx ctx, const natusChar *str, size_t len, natusEngValFlags *flags)
{
  jsval v = JSVAL_VOID;
  JSString *s = JS_NewUCStringCopyN(ctx, str, len);
  if (s)
    v = STRING_TO_JSVAL(s);
  else {
    if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
      *flags |= natusEngValFlagException;
  }

  return mkjsval(ctx, STRING_TO_JSVAL(s));
}

static natusEngVal
sm_new_array(const natusEngCtx ctx, const natusEngVal *array, size_t len, natusEngValFlags *flags)
{
  jsval *valv = calloc(len, sizeof(jsval));
  if (!valv)
    return NULL;

  int i;
  for (i = 0; i < len; i++)
    valv[i] = *array[i];

  JSObject* obj = JS_NewArrayObject(ctx, i, valv);
  free(valv);

  jsval v;
  if (obj)
    v = OBJECT_TO_JSVAL(obj);
  else if (JS_IsExceptionPending(ctx) && JS_GetPendingException(ctx, &v))
    *flags |= natusEngValFlagException;

  return mkjsval(ctx, v);
}

static natusEngVal
sm_new_function(const natusEngCtx ctx, const char *name, natusPrivate *priv, natusEngValFlags *flags)
{
  JSFunction *fnc = JS_NewFunction(ctx, fnc_call, 0, JSFUN_CONSTRUCTOR, NULL, name);
  JSObject *obj = JS_GetFunctionObject(fnc);
  JSObject *prv = JS_NewObject(ctx, &fncdef, NULL, NULL);

  jsval v = JSVAL_VOID;
  if (fnc && obj && prv && JS_SetReservedSlot(ctx, obj, 0, OBJECT_TO_JSVAL(prv)) && JS_SetPrivate(ctx, prv, priv))
    v = OBJECT_TO_JSVAL(obj);
  else {
    natus_private_free(priv);
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &v))
      return NULL;
    *flags |= natusEngValFlagException;
  }

  return mkjsval(ctx, v);
}

static natusEngVal
sm_new_object(const natusEngCtx ctx, natusClass *cls, natusPrivate *priv, natusEngValFlags *flags)
{
  JSClass *jscls = NULL;
  if (cls) {
    jscls = malloc(sizeof(JSClass));
    if (!jscls)
      goto error;

    memset(jscls, 0, sizeof(JSClass));
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

    if (!natus_private_push(priv, jscls, free))
      goto error;
  }

  // Build the object
  jsval v = JSVAL_VOID;
  JSObject* obj = JS_NewObject(ctx, jscls ? jscls : &objdef, NULL, NULL);
  if (obj && JS_SetPrivate(ctx, obj, priv))
    v = OBJECT_TO_JSVAL(obj);
  else {
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &v))
      goto error;
    *flags |= natusEngValFlagException;
    natus_private_free(priv);
  }

  return mkjsval(ctx, v);

error:
  natus_private_free(priv);
  return NULL;
}

static natusEngVal
sm_new_null(const natusEngCtx ctx, natusEngValFlags *flags)
{
  return mkjsval(ctx, JSVAL_NULL);
}

static natusEngVal
sm_new_undefined(const natusEngCtx ctx, natusEngValFlags *flags)
{
  return mkjsval(ctx, JSVAL_VOID);
}

static bool
sm_to_bool(const natusEngCtx ctx, const natusEngVal val)
{
  return JSVAL_TO_BOOLEAN(*val);
}

static double
sm_to_double(const natusEngCtx ctx, const natusEngVal val)
{
  double d;
  JS_ValueToNumber(ctx, *val, &d);
  return d;
}

static char *
sm_to_string_utf8(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  JSString *str = JS_ValueToString(ctx, *val);
  *len = JS_GetStringLength(str);

  size_t bufflen = JS_GetStringEncodingLength(ctx, str);
  char *buff = calloc(bufflen + 1, sizeof(char));
  if (!buff)
    return NULL;
  memset(buff, 0, sizeof(char) * (bufflen + 1));

  *len = JS_EncodeStringToBuffer(str, buff, bufflen);
  if (*len >= 0 && *len <= bufflen)
    return buff;
  free(buff);
  return NULL;
}

static natusChar *
sm_to_string_utf16(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  JSString *str = JS_ValueToString(ctx, *val);
  *len = JS_GetStringLength(str);

  const jschar *jschars = JS_GetStringCharsAndLength(ctx, str, len);
  natusChar *ntchars = calloc(*len + 1, sizeof(natusChar));
  if (jschars && ntchars) {
    memset(ntchars, 0, sizeof(natusChar) * (*len + 1));
    memcpy(ntchars, jschars, sizeof(natusChar) * *len);
    return ntchars;
  }

  free(ntchars);
  return NULL;
}

natusEngVal
sm_del(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  jsid vid;
  jsval rval = JSVAL_VOID;

  JS_ValueToId(ctx, *id, &vid);

  if (JS_DeletePropertyById(ctx, JSVAL_TO_OBJECT(*val), vid))
    rval = BOOLEAN_TO_JSVAL(true);
  else {
    *flags |= natusEngValFlagException;
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval))
      return NULL;
  }

  return mkjsval(ctx, rval);
}

natusEngVal
sm_get(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  jsid vid;
  JS_ValueToId(ctx, *id, &vid);

  jsval rval = JSVAL_VOID;
  if (!JS_GetPropertyById(ctx, JSVAL_TO_OBJECT(*val), vid, &rval)) {
    *flags |= natusEngValFlagException;
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval))
      return NULL;
  }

  return mkjsval(ctx, rval);
}

natusEngVal
sm_set(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, const natusEngVal value, natusPropAttr attrs, natusEngValFlags *flags)
{
  jsint attr = (attrs & natusPropAttrDontEnum) ? 0 : JSPROP_ENUMERATE;
  attr |= (attrs & natusPropAttrReadOnly) ? JSPROP_READONLY : 0;
  attr |= (attrs & natusPropAttrDontDelete) ? JSPROP_PERMANENT : 0;

  jsid vid;
  JS_ValueToId(ctx, *id, &vid);

  jsval rval = *value;
  if (JS_SetPropertyById(ctx, JSVAL_TO_OBJECT(*val), vid, &rval)) {
    BOOLEAN_TO_JSVAL(true);

    if (JSID_IS_STRING(vid)) {
      size_t len;
      JSBool found;
      JSString *str = JSID_TO_STRING(vid);
      const jschar *chars = JS_GetStringCharsAndLength(ctx, str, &len);
      JS_SetUCPropertyAttributes(ctx, JSVAL_TO_OBJECT(*val), chars, len, attr, &found);
    }
  } else {
    *flags |= natusEngValFlagException;
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval))
      return NULL;
  }

  return mkjsval(ctx, rval);
}

natusEngVal
sm_enumerate(const natusEngCtx ctx, natusEngVal val, natusEngValFlags *flags)
{
  jsint i;

  JSIdArray* na = JS_Enumerate(ctx, JSVAL_TO_OBJECT(*val));
  if (!na)
    return NULL;

  jsval *vals = calloc(na->length, sizeof(jsval));
  if (!vals) {
    JS_DestroyIdArray(ctx, na);
    return NULL;
  }

  for (i = 0; i < na->length; i++)
    assert(JS_IdToValue (ctx, na->vector[i], &vals[i]));

  JSObject *array = JS_NewArrayObject(ctx, na->length, vals);
  JS_DestroyIdArray(ctx, na);
  free(vals);
  if (!array)
    return NULL;

  return mkjsval(ctx, OBJECT_TO_JSVAL(array));
}

static natusEngVal
sm_call(const natusEngCtx ctx, natusEngVal func, natusEngVal ths, natusEngVal args, natusEngValFlags *flags)
{
  // Convert to jsval array
  jsuint i, len;
  if (!JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(*args), &len))
    return NULL;

  jsval *argv = calloc(len, sizeof(jsval));
  if (!argv)
    return NULL;

  for (i = 0; i < len; i++) {
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
    JSObject *obj = JS_New(ctx, JSVAL_TO_OBJECT(*func), len, argv);
    if (obj)
      rval = OBJECT_TO_JSVAL(obj);
    else {
      *flags |= natusEngValFlagException;
      if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
        free(argv);
        return NULL;
      }
    }
  } else {
    if (!JS_CallFunctionValue(ctx, JSVAL_TO_OBJECT(*ths), *func, len, argv, &rval)) {
      *flags |= natusEngValFlagException;
      if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
        free(argv);
        return NULL;
      }
    }
  }
  free(argv);

  return mkjsval(ctx, rval);
}

static natusEngVal
sm_evaluate(const natusEngCtx ctx, natusEngVal ths, const natusEngVal jscript, const natusEngVal filename, unsigned int lineno, natusEngValFlags *flags)
{
  size_t jslen = 0, fnlen = 0;
  const jschar *jschars = JS_GetStringCharsAndLength(ctx, JS_ValueToString(ctx, *jscript), &jslen);
  char *fnchars = sm_to_string_utf8(ctx, filename, &fnlen);

  jsval rval;
  if (!JS_EvaluateUCScript(ctx, JSVAL_TO_OBJECT(*ths), jschars, jslen, fnchars, lineno, &rval)) {
    *flags |= natusEngValFlagException;
    if (!JS_IsExceptionPending(ctx) || !JS_GetPendingException(ctx, &rval)) {
      free(fnchars);
      return NULL;
    }
  }

  free(fnchars);
  return mkjsval(ctx, rval);
}

static natusPrivate *
sm_get_private(const natusEngCtx ctx, const natusEngVal val)
{
  return get_private(ctx, JSVAL_TO_OBJECT(*val));
}

static natusEngVal
sm_get_global(const natusEngCtx ctx, const natusEngVal val, natusEngValFlags *flags)
{
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

static natusValueType
sm_get_type(const natusEngCtx ctx, const natusEngVal val)
{
  if (JSVAL_IS_BOOLEAN(*val))
    return natusValueTypeBoolean;
  else if (JSVAL_IS_NULL(*val))
    return natusValueTypeNull;
  else if (JSVAL_IS_NUMBER(*val))
    return natusValueTypeNumber;
  else if (JSVAL_IS_STRING(*val))
    return natusValueTypeString;
  else if (JSVAL_IS_VOID(*val))
    return natusValueTypeUndefined;
  else if (JSVAL_IS_OBJECT(*val)) {
    if (JS_IsArrayObject(ctx, JSVAL_TO_OBJECT(*val)))
      return natusValueTypeArray;
    else if (JS_ObjectIsFunction(ctx, JSVAL_TO_OBJECT(*val)))
      return natusValueTypeFunction;
    else
      return natusValueTypeObject;
  }
  return natusValueTypeUnknown;
}

static bool
sm_borrow_context(natusEngCtx ctx, natusEngVal val, void **context, void **value)
{
  *context = ctx;
  *value = val;
  return true;
}

static bool
sm_equal(const natusEngCtx ctx, const natusEngVal val1, const natusEngVal val2, bool strict)
{
  JSBool eql = false;

  if (strict) {
    if (!JS_StrictlyEqual(ctx, *val1, *val2, &eql))
      eql = false;
  } else {
#if 1
    if (!JS_StrictlyEqual(ctx, *val1, *val2, &eql))
#else
    if (!JS_LooselyEqual(ctx, *val1, *val2, &eql))
#endif
      eql = false;
  }

  return eql;
}

__attribute__((constructor))
static void
_init()
{
  JS_SetCStringsAreUTF8();
}

__attribute__((destructor))
static void
_fini()
{
  JS_ShutDown();
}

NATUS_ENGINE("SpiderMonkey", "JS_GetProperty", sm);
