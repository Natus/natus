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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef __APPLE__
// JavaScriptCore.h requires CoreFoundation
// This is only found on Mac OS X
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

typedef JSContextRef natusEngCtx;
typedef JSValueRef natusEngVal;
#define NATUS_ENGINE_TYPES_DEFINED
#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-engine.h>

#define NATUS_PRIV_JSC_CLASS "natus.JavaScriptCore.JSClass"

#define checkerror() \
  if (exc) { \
    *flags |= natusEngValFlagException; \
    return exc; \
  }

#define checkerrorval(val) \
  checkerror(); \
  if (!val) \
    return NULL

typedef struct {
  JSClassDefinition def;
  JSClassRef ref;
} JavaScriptCoreClass;

static void
class_free(JavaScriptCoreClass *jscc)
{
  if (!jscc)
    return;
  if (jscc->ref)
    JSClassRelease(jscc->ref);
  free(jscc);
}

static size_t
pows(size_t base, size_t pow)
{
  if (pow == 0)
    return 1;
  return base * pows(base, pow - 1);
}

static natusEngVal
property_to_value(natusEngCtx ctx, JSStringRef propertyName)
{
  size_t i = 0, idx = 0, len = JSStringGetLength(propertyName);
  const JSChar *jschars = JSStringGetCharactersPtr(propertyName);
  if (!jschars)
    return NULL;

  for (; jschars && len-- > 0; i++) {
    if (jschars[len] < '0' || jschars[len] > '9')
      goto str;

    idx += (jschars[len] - '0') * pows(10, i);
  }
  if (i > 0)
    return JSValueMakeNumber(ctx, idx);

str:
  return JSValueMakeString(ctx, propertyName);
}

static void
obj_finalize(JSObjectRef object)
{
  natus_private_free(JSObjectGetPrivate(object));
}

static bool
obj_del(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc)
{
  natusEngVal idx = property_to_value(ctx, propertyName);
  if (idx) {
    natusEngValFlags flags = natusEngValFlagNone;
    JSValueProtect(ctx, object);
    JSValueProtect(ctx, idx);
    JSValueRef ret = natus_handle_property(natusPropertyActionDelete, object, JSObjectGetPrivate(object), idx, NULL, &flags);
    if (flags & natusEngValFlagMustFree)
      JSValueUnprotect(ctx, ret);
    if (flags & natusEngValFlagException) {
      *exc = ret;
      return false;
    }

    return JSValueIsBoolean(ctx, ret) ? JSValueToBoolean(ctx, ret) : false;
  }
  return false;
}

static JSValueRef
obj_get(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exc)
{
  natusEngVal idx = property_to_value(ctx, propertyName);
  if (idx) {
    natusEngValFlags flags = natusEngValFlagNone;
    JSValueProtect(ctx, object);
    JSValueProtect(ctx, idx);
    JSValueRef ret = natus_handle_property(natusPropertyActionGet, object, JSObjectGetPrivate(object), idx, NULL, &flags);
    if (flags & natusEngValFlagMustFree)
      JSValueUnprotect(ctx, ret);
    if (flags & natusEngValFlagException) {
      *exc = ret;
      return NULL;
    }

    return ret;
  }
  return NULL;
}

static bool
obj_set(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exc)
{
  natusEngVal idx = property_to_value(ctx, propertyName);
  if (idx) {
    natusEngValFlags flags = natusEngValFlagNone;
    JSValueProtect(ctx, object);
    JSValueProtect(ctx, idx);
    JSValueProtect(ctx, value);
    JSValueRef ret = natus_handle_property(natusPropertyActionSet, object, JSObjectGetPrivate(object), idx, value, &flags);
    if (flags & natusEngValFlagMustFree)
      JSValueUnprotect(ctx, ret);
    if (flags & natusEngValFlagException) {
      *exc = ret;
      return false;
    }

    return JSValueIsBoolean(ctx, ret) ? JSValueToBoolean(ctx, ret) : false;
  }
  return false;
}

static void
obj_enum(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames)
{
  JSValueRef exc = NULL;

  natusEngValFlags flags = natusEngValFlagNone;
  JSValueRef rslt = natus_handle_property(natusPropertyActionEnumerate, JSObjectGetPrivate(object), NULL, NULL, NULL, &flags);
  if (!rslt)
    return;
  if (flags & natusEngValFlagMustFree)
    JSValueUnprotect(ctx, rslt);

  JSObjectRef obj = JSValueToObject(ctx, rslt, &exc);
  if (!obj || exc)
    return;

  JSStringRef length = JSStringCreateWithUTF8CString("length");
  if (!length)
    return;
  JSValueRef len = JSObjectGetProperty(ctx, obj, length, &exc);
  JSStringRelease(length);
  if (!len || exc)
    return;

  int count = (int) JSValueToNumber(ctx, len, &exc);
  if (exc)
    return;

  int i;
  for (i = 0; i < count; i++) {
    JSValueRef val = JSObjectGetPropertyAtIndex(ctx, obj, i, &exc);
    if (!val || exc)
      return;

    JSStringRef str = JSValueToStringCopy(ctx, val, NULL);
    if (!str)
      return;

    JSPropertyNameAccumulatorAddName(propertyNames, str);
    JSStringRelease(str);
  }
}

static JSValueRef
obj_call(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argc, const JSValueRef args[], JSValueRef* exc)
{
  natusEngValFlags flags = natusEngValFlagNone;
  JSValueRef arg = JSObjectMakeArray(ctx, argc, args, exc);
  JSValueProtect(ctx, object);
  if (thisObject)
    JSValueProtect(ctx, thisObject);
  JSValueProtect(ctx, arg);
  JSValueRef ret = natus_handle_call(object, JSObjectGetPrivate(object), thisObject, arg, &flags);
  if (!ret) {
    *exc = JSValueMakeUndefined(ctx);
    return NULL;
  }

  if (flags & natusEngValFlagMustFree)
    JSValueUnprotect(ctx, ret);

  if (flags & natusEngValFlagException) {
    *exc = ret;
    return NULL;
  }

  return ret;
}

static JSObjectRef
obj_new(JSContextRef ctx, JSObjectRef object, size_t argc, const JSValueRef args[], JSValueRef* exc)
{
  return (JSObjectRef) obj_call(ctx, object, NULL, argc, args, exc);
}

static JSClassRef glbcls;
static JSClassRef objcls;
static JSClassRef fnccls;
static JSClassDefinition glbclassdef = {
    .className = "GlobalObject",
    .finalize = obj_finalize,
};
static JSClassDefinition objclassdef = {
    .className = "NativeObject",
    .finalize = obj_finalize,
};
static JSClassDefinition fncclassdef = {
    .className = "NativeFunction",
    .finalize = obj_finalize,
    .callAsFunction = obj_call,
    .callAsConstructor = obj_new,
};

__attribute__((constructor))
static void
_init()
{
  // Make sure the enum values don't get changed on us
  assert(kJSPropertyAttributeNone == (JSPropertyAttributes) natusPropAttrNone);
  assert(kJSPropertyAttributeReadOnly == (JSPropertyAttributes) natusPropAttrReadOnly << 1);
  assert(kJSPropertyAttributeDontEnum == (JSPropertyAttributes) natusPropAttrDontEnum << 1);
  assert(kJSPropertyAttributeDontDelete == (JSPropertyAttributes) natusPropAttrDontDelete << 1);

  if (!glbcls)
    assert(glbcls = JSClassCreate(&glbclassdef));
  if (!objcls)
    assert(objcls = JSClassCreate(&objclassdef));
  if (!fnccls)
    assert(fnccls = JSClassCreate(&fncclassdef));
}

__attribute__((destructor))
static void
_fini()
{
  if (glbcls) {
    JSClassRelease(glbcls);
    glbcls = NULL;
  }
  if (objcls) {
    JSClassRelease(objcls);
    objcls = NULL;
  }
  if (fnccls) {
    JSClassRelease(fnccls);
    fnccls = NULL;
  }
}

static void
jsc_ctx_free(natusEngCtx ctx)
{
  JSGarbageCollect(ctx);
  JSGlobalContextRelease((JSGlobalContextRef) ctx);
}

static void
jsc_val_unlock(natusEngCtx ctx, natusEngVal val)
{
  if (val != JSContextGetGlobalObject(ctx))
    JSValueUnprotect(ctx, val);
}

static natusEngVal
jsc_val_duplicate(natusEngCtx ctx, natusEngVal val)
{
  JSValueProtect(ctx, val);
  return val;
}

static void
jsc_val_free(natusEngVal val)
{

}

static natusEngVal
jsc_new_global(natusEngCtx ctx, natusEngVal val, natusPrivate *priv, natusEngCtx *newctx, natusEngValFlags *flags)
{
  *newctx = NULL;
  *flags = natusEngValFlagNone;

  JSContextGroupRef grp = NULL;
  if (ctx)
    grp = JSContextGetGroup(ctx);

  *newctx = JSGlobalContextCreateInGroup(grp, glbcls);
  if (!*newctx)
    goto error;

  JSObjectRef glb = JSContextGetGlobalObject(*newctx);
  if (!glb)
    goto error;

  if (!JSObjectSetPrivate(glb, priv))
    goto error;

  return glb;

error:
  if (*newctx) {
    JSGlobalContextRelease((JSGlobalContextRef) *newctx);
    *newctx = NULL;
  }
  natus_private_free(priv);
  return NULL;
}

static natusEngVal
jsc_new_bool(const natusEngCtx ctx, bool b, natusEngValFlags *flags)
{
  return JSValueMakeBoolean(ctx, b);
}

static natusEngVal
jsc_new_number(const natusEngCtx ctx, double n, natusEngValFlags *flags)
{
  return JSValueMakeNumber(ctx, n);
}

static natusEngVal
jsc_new_string_utf8(const natusEngCtx ctx, const char *str, size_t len, natusEngValFlags *flags)
{
  JSStringRef string = JSStringCreateWithUTF8CString(str);
  JSValueRef val = JSValueMakeString(ctx, string);
  JSStringRelease(string);
  return val;
}

static natusEngVal
jsc_new_string_utf16(const natusEngCtx ctx, const natusChar *str, size_t len, natusEngValFlags *flags)
{
  JSStringRef string = JSStringCreateWithCharacters(str, len);
  JSValueRef val = JSValueMakeString(ctx, string);
  JSStringRelease(string);
  return val;
}

static natusEngVal
jsc_new_array(const natusEngCtx ctx, const natusEngVal *array, size_t len, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;
  JSObjectRef ret = JSObjectMakeArray(ctx, len, array, &exc);
  checkerrorval(ret);
  return ret;
}

static natusEngVal
jsc_new_function(const natusEngCtx ctx, const char *name, natusPrivate *priv, natusEngValFlags *flags)
{
  JSStringRef stmp;
  JSValueRef exc = NULL;

  // Get the function object and the prototype
  JSObjectRef global = JSContextGetGlobalObject(ctx);
  checkerrorval(global);

  stmp = JSStringCreateWithUTF8CString("Function");
  checkerrorval(stmp);
  JSValueRef function = JSObjectGetProperty(ctx, global, stmp, &exc);
  JSStringRelease(stmp);
  checkerrorval(function);

  JSObjectRef funcobj = JSValueToObject(ctx, function, &exc);
  checkerrorval(funcobj);
  stmp = JSStringCreateWithUTF8CString("prototype");
  checkerrorval(stmp);
  JSValueRef prototype = JSObjectGetProperty(ctx, funcobj, stmp, &exc);
  JSStringRelease(stmp);
  checkerrorval(prototype);

  // Make our new function (which is really an object we will convert to emulate a function)
  JSObjectRef obj = JSObjectMake(ctx, fnccls, priv);
  checkerrorval(obj);

  // Convert the object to emulate a function
  JSObjectSetPrototype(ctx, obj, prototype);

  // Add a proper toString() function
  JSStringRef fname = JSStringCreateWithUTF8CString("toString");
  checkerrorval(fname);
  stmp = JSStringCreateWithUTF8CString("return 'function ' + this.name + '() {\\n    [native code]\\n}';");
  if (!stmp) {
    JSStringRelease(fname);
    return NULL;
  }
  JSObjectRef toString = JSObjectMakeFunction(ctx, fname, 0, NULL, stmp, NULL, 0, &exc);
  JSStringRelease(stmp);
  if (exc) {
    JSStringRelease(fname);
    *flags |= natusEngValFlagException;
    return exc;
  }
  JSObjectSetProperty(ctx, obj, fname, toString, kJSPropertyAttributeNone, &exc);
  JSStringRelease(fname);
  checkerror();

  // Set the name
  fname = JSStringCreateWithUTF8CString(name ? name : "anonymous");
  checkerrorval(fname);
  stmp = JSStringCreateWithUTF8CString("name");
  if (!stmp) {
    JSStringRelease(fname);
    return NULL;
  }
  JSObjectSetProperty(ctx, obj, stmp, JSValueMakeString(ctx, fname), kJSPropertyAttributeNone, &exc);
  JSStringRelease(stmp);
  JSStringRelease(fname);
  checkerror();

  stmp = JSStringCreateWithUTF8CString("constructor");
  checkerrorval(stmp);
  JSObjectSetProperty(ctx, obj, stmp, function, kJSPropertyAttributeNone, &exc);
  JSStringRelease(stmp);
  checkerror();

  return obj;
}

static natusEngVal
jsc_new_object(const natusEngCtx ctx, natusClass *cls, natusPrivate *priv, natusEngValFlags *flags)
{
  // Fill in our functions
  JSClassRef jscls = objcls;
  if (cls) {
    JavaScriptCoreClass *jscc = malloc(sizeof(JavaScriptCoreClass));
    if (!jscc)
      return NULL;

    memset(jscc, 0, sizeof(JavaScriptCoreClass));
    jscc->def.finalize = obj_finalize;
    jscc->def.className = cls->call ? "NativeFunction" : "NativeObject";
    jscc->def.getProperty = cls->get ? obj_get : NULL;
    jscc->def.setProperty = cls->set ? obj_set : NULL;
    jscc->def.deleteProperty = cls->del ? obj_del : NULL;
    jscc->def.getPropertyNames = cls->enumerate ? obj_enum : NULL;
    jscc->def.callAsFunction = cls->call ? obj_call : NULL;
    jscc->def.callAsConstructor = cls->call ? obj_new : NULL;

    if (!natus_private_set(priv, NATUS_PRIV_JSC_CLASS, jscc, (natusFreeFunction) class_free))
      return NULL;

    jscc->ref = jscls = JSClassCreate(&jscc->def);
    if (!jscls)
      return NULL;
  }

  // Build the object
  JSObjectRef obj = JSObjectMake(ctx, jscls, priv);
  if (!obj)
    return NULL;

  // Do the return
  return obj;
}

static natusEngVal
jsc_new_null(const natusEngCtx ctx, natusEngValFlags *flags)
{
  return JSValueMakeNull(ctx);
}

static natusEngVal
jsc_new_undefined(const natusEngCtx ctx, natusEngValFlags *flags)
{
  return JSValueMakeUndefined(ctx);
}

static bool
jsc_to_bool(const natusEngCtx ctx, const natusEngVal val)
{
  return JSValueToBoolean(ctx, val);
}

static double
jsc_to_double(const natusEngCtx ctx, const natusEngVal val)
{
  return JSValueToNumber(ctx, val, NULL);
}

static char *
jsc_to_string_utf8(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  JSStringRef str = JSValueToStringCopy(ctx, val, NULL);
  if (!str)
    return NULL;

  *len = JSStringGetMaximumUTF8CStringSize(str);
  char *buff = calloc(*len, sizeof(char));
  if (!buff) {
    JSStringRelease(str);
    return NULL;
  }

  *len = JSStringGetUTF8CString(str, buff, *len);
  if (*len > 0)
    *len -= 1;
  JSStringRelease(str);
  return buff;
}

static natusChar *
jsc_to_string_utf16(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  JSStringRef str = JSValueToStringCopy(ctx, val, NULL);
  if (!str)
    return NULL;

  *len = JSStringGetLength(str);
  natusChar *buff = calloc(*len + 1, sizeof(natusChar));
  if (!buff) {
    JSStringRelease(str);
    return NULL;
  }
  memcpy(buff, JSStringGetCharactersPtr(str), *len * sizeof(JSChar));
  buff[*len] = '\0';
  JSStringRelease(str);

  return buff;
}

static natusEngVal
jsc_del(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;

  JSObjectRef obj = JSValueToObject(ctx, val, &exc);
  checkerrorval(obj);

  JSStringRef sidx = JSValueToStringCopy(ctx, id, &exc);
  checkerrorval(sidx);

  JSObjectDeleteProperty(ctx, obj, sidx, &exc);
  JSStringRelease(sidx);
  checkerror();

  return JSValueMakeBoolean(ctx, true);
}

static natusEngVal
jsc_get(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;
  JSValueRef rslt = NULL;

  JSObjectRef obj = JSValueToObject(ctx, val, &exc);
  checkerrorval(obj);

  if (JSValueIsNumber(ctx, id)) {
    double idx = JSValueToNumber(ctx, id, &exc);
    checkerror();
    rslt = JSObjectGetPropertyAtIndex(ctx, obj, idx, &exc);
  } else {
    JSStringRef idx = JSValueToStringCopy(ctx, id, &exc);
    checkerrorval(idx);
    rslt = JSObjectGetProperty(ctx, obj, idx, &exc);
    JSStringRelease(idx);
  }
  checkerrorval(rslt);
  return rslt;
}

static natusEngVal
jsc_set(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, const natusEngVal value, natusPropAttr attrs, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;

  JSObjectRef obj = JSValueToObject(ctx, val, &exc);
  checkerrorval(obj);

  if (JSValueIsNumber(ctx, id)) {
    double idx = JSValueToNumber(ctx, id, &exc);
    if (!exc)
      JSObjectSetPropertyAtIndex(ctx, obj, idx, value, &exc);
  } else {
    JSStringRef idx = JSValueToStringCopy(ctx, id, &exc);
    if (idx) {
      JSPropertyAttributes flags = attrs == natusPropAttrNone ? natusPropAttrNone : attrs << 1;
      JSObjectSetProperty(ctx, obj, idx, value, flags, &exc);
      JSStringRelease(idx);
    }
  }
  checkerror();
  return JSValueMakeBoolean(ctx, true);
}

static natusEngVal
jsc_enumerate(const natusEngCtx ctx, natusEngVal val, natusEngValFlags *flags)
{
  size_t i;
  JSValueRef exc = NULL;

  JSObjectRef obj = JSValueToObject(ctx, val, &exc);
  checkerrorval(obj);

  // Get the names
  JSPropertyNameArrayRef na = JSObjectCopyPropertyNames(ctx, obj);
  checkerrorval(na);

  size_t c = JSPropertyNameArrayGetCount(na);
  JSValueRef *items = malloc(sizeof(JSValueRef *) * c);
  if (!items) {
    JSPropertyNameArrayRelease(na);
    return NULL;
  }

  for (i = 0; i < c; i++) {
    JSStringRef name = JSPropertyNameArrayGetNameAtIndex(na, i);
    if (name) {
      items[i] = JSValueMakeString(ctx, name);
      if (items[i])
        continue;
    }

    c = i;
    break;
  }
  JSPropertyNameArrayRelease(na);

  JSObjectRef array = JSObjectMakeArray(ctx, c, items, &exc);
  free(items);
  checkerror();
  return array;
}

static natusEngVal
jsc_call(const natusEngCtx ctx, natusEngVal func, natusEngVal ths, natusEngVal args, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;

  JSObjectRef funcobj = JSValueToObject(ctx, func, &exc);
  checkerrorval(funcobj);

  JSObjectRef argsobj = JSValueToObject(ctx, args, &exc);
  checkerrorval(argsobj);

  // Convert to jsval array
  JSStringRef name = JSStringCreateWithUTF8CString("length");
  checkerrorval(name);
  JSValueRef length = JSObjectGetProperty(ctx, argsobj, name, &exc);
  checkerrorval(length);
  long i, len = JSValueToNumber(ctx, length, &exc);
  checkerror();

  JSValueRef *argv = calloc(len, sizeof(JSValueRef *));
  if (!argv)
    return NULL;
  for (i = 0; i < len; i++) {
    argv[i] = JSObjectGetPropertyAtIndex(ctx, argsobj, i, &exc);
    if (!argv[i] || exc) {
      free(argv);
      *flags |= natusEngValFlagException;
      return exc;
    }
  }

  // Call the function
  JSValueRef rval;
  if (JSValueIsUndefined(ctx, ths))
    rval = JSObjectCallAsConstructor(ctx, funcobj, i, argv, &exc);
  else {
    JSObjectRef thsobj = JSValueToObject(ctx, ths, &exc);
    checkerrorval(thsobj);
    rval = JSObjectCallAsFunction(ctx, funcobj, thsobj, i, argv, &exc);
  }
  free(argv);
  checkerror();
  return rval;
}

static natusEngVal
jsc_evaluate(const natusEngCtx ctx, natusEngVal ths, const natusEngVal jscript, const natusEngVal filename, unsigned int lineno, natusEngValFlags *flags)
{
  JSValueRef exc = NULL;

  JSObjectRef thsobj = JSValueToObject(ctx, ths, &exc);
  checkerrorval(thsobj);

  JSStringRef strjscript = JSValueToStringCopy(ctx, jscript, &exc);
  checkerrorval(strjscript);

  JSStringRef strfilename = JSValueToStringCopy(ctx, filename, &exc);
  if (!strfilename || exc)
    JSStringRelease(strjscript);
  checkerrorval(strfilename);

  JSValueRef rval = JSEvaluateScript(ctx, strjscript, thsobj, strfilename, lineno, &exc);
  JSStringRelease(strjscript);
  JSStringRelease(strfilename);
  checkerror();
  return rval;
}

static natusPrivate *
jsc_get_private(const natusEngCtx ctx, const natusEngVal val)
{
  JSObjectRef obj = JSValueToObject(ctx, val, NULL);
  if (!obj)
    return NULL;
  return JSObjectGetPrivate(obj);
}

static natusEngVal
jsc_get_global(const natusEngCtx ctx, const natusEngVal val)
{
  return JSContextGetGlobalObject(ctx);
}

static natusValueType
jsc_get_type(const natusEngCtx ctx, const natusEngVal val)
{
  JSType type = JSValueGetType(ctx, val);
  switch (type) {
  case kJSTypeBoolean:
    return natusValueTypeBoolean;
  case kJSTypeNull:
    return natusValueTypeNull;
  case kJSTypeNumber:
    return natusValueTypeNumber;
  case kJSTypeString:
    return natusValueTypeString;
  case kJSTypeObject:
    break;
  default:
    return natusValueTypeUndefined;
  }

  JSObjectRef obj = JSValueToObject(ctx, val, NULL);
  assert(obj);

  natusPrivate *priv = JSObjectGetPrivate(obj);

  // FUNCTION
  if (JSObjectIsFunction(ctx, obj) && (!priv || JSValueIsObjectOfClass(ctx, val, fnccls)))
    return natusValueTypeFunction;

  // Bypass infinite loops for objects defined by natus
  // (otherwise, the JSObjectGetProperty() calls below will
  //  call the natus defined property handler, which may call
  //  get_type(), etc)
  if (JSObjectGetPrivate(obj))
    return natusValueTypeObject;

  // ARRAY
  /* Yes, this is really ugly. But unfortunately JavaScriptCore is missing JSValueIsArray().
   * We also can't check the constructor directly for identity with the Array object because
   * we can't get the global object associated with this value (the global for this context
   * may not be the global for this value).
   * Dear JSC, please add JSValueIsArray() (and also JSValueGetGlobal())... */
  JSStringRef str = JSStringCreateWithUTF8CString("constructor");
  if (!str)
    return natusValueTypeObject;
  JSValueRef prp = JSObjectGetProperty(ctx, obj, str, NULL);
  JSStringRelease(str);
  JSObjectRef prpobj = JSValueIsObject(ctx, prp) ? JSValueToObject(ctx, prp, NULL) : NULL;
  str = JSStringCreateWithUTF8CString("name");
  if (!str)
    return natusValueTypeObject;
  if (prpobj && (prp = JSObjectGetProperty(ctx, prpobj, str, NULL))
      && JSValueIsString(ctx, prp)) {
    JSStringRelease(str);
    str = JSValueToStringCopy(ctx, prp, NULL);
    if (!str)
      return natusValueTypeObject;
    if (JSStringIsEqualToUTF8CString(str, "Array")) {
      JSStringRelease(str);
      return natusValueTypeArray;
    }
  }
  JSStringRelease(str);

  // OBJECT
  return natusValueTypeObject;
}

static bool
jsc_borrow_context(natusEngCtx ctx, natusEngVal val, void **context, void **value)
{
  *context = (void*) ctx;
  *value = (void*) val;
  return true;
}

static bool
jsc_equal(const natusEngCtx ctx, const natusEngVal val1, const natusEngVal val2, bool strict)
{
  if (strict)
    return JSValueIsStrictEqual(ctx, val1, val2);
  return JSValueIsEqual(ctx, val1, val2, NULL);
}

NATUS_ENGINE("JavaScriptCore", "JSObjectMakeFunctionWithCallback", jsc);
