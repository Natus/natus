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

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <new>

#include <v8.h>
using namespace v8;

typedef Persistent<Context>* natusEngCtx;
typedef Persistent<Value>* natusEngVal;
#define NATUS_ENGINE_TYPES_DEFINED
#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-engine.h>

#define V8_PRIV_SLOT 0
#define V8_PRIV_STRING String::New("natus::v8::private")

static natusEngVal
makeval(Handle<Value> val)
{
  natusEngVal tmp = new Persistent<Value>;
  *tmp = Persistent<Value>::New(val);
  return tmp;
}

static void
on_free(Persistent<Value> object, void* parameter)
{
  natus_private_free((natusPrivate*) parameter);
  object.Dispose();
  object.ClearWeak();
}

static Handle<Value>
property_handler(const AccessorInfo& info, Handle<Value> property, Handle<Value> value, natusPropertyAction act)
{
  HandleScope hs;

  natusPrivate *priv = (natusPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);

  natusEngValFlags flags = natusEngValFlagNone;
  natusEngVal ths = makeval(info.This());
  natusEngVal idx = makeval(property);
  natusEngVal val = (act & natusPropertyActionSet) ? makeval(value) : NULL;
  natusEngVal res = natus_handle_property(act, ths, priv, idx, val, &flags);
  if (!res)
    return Handle<Value>();

  Handle<Value> ret = *res;
  if (flags & natusEngValFlagUnlock) {
    res->Dispose();
    res->Clear();
  }
  if (flags & natusEngValFlagFree)
    delete res;

  if (flags & natusEngValFlagException)
    return ret->IsUndefined() ? Handle<Value>() : ThrowException(ret);

  if (act & natusPropertyActionGet)
    return ret;

  return Boolean::New(true);
}

static Handle<Value>
obj_item_get(uint32_t index, const AccessorInfo& info)
{
  return property_handler(info, Uint32::New(index), Handle<Value>(), natusPropertyActionGet);
}

static Handle<Value>
obj_property_get(Local<String> property, const AccessorInfo& info)
{
  return property_handler(info, property, Handle<Value>(), natusPropertyActionGet);
}

static Handle<Boolean>
obj_item_del(uint32_t index, const AccessorInfo& info)
{
  Handle<Value> v = property_handler(info, Uint32::New(index), Handle<Value>(), natusPropertyActionDelete);
  if (v.IsEmpty())
    return Handle<Boolean>();
  return v->ToBoolean();
}

static Handle<Boolean>
obj_property_del(Local<String> property, const AccessorInfo& info)
{
  Handle<Value> v = property_handler(info, property, Handle<Value>(), natusPropertyActionDelete);
  if (v.IsEmpty())
    return Handle<Boolean>();
  return v->ToBoolean();
}

static Handle<Value>
obj_item_set(uint32_t index, Local<Value> value, const AccessorInfo& info)
{
  return property_handler(info, Uint32::New(index), value, natusPropertyActionSet);
}

static Handle<Value>
obj_property_set(Local<String> property, Local<Value> value, const AccessorInfo& info)
{
  return property_handler(info, property, value, natusPropertyActionSet);
}

static Handle<Array>
obj_enumerate(const AccessorInfo& info)
{
  HandleScope hs;

  natusEngValFlags flags = natusEngValFlagNone;
  natusPrivate* prv = (natusPrivate*) External::Unwrap(info.Data()->ToObject());
  natusEngVal res = natus_handle_property(natusPropertyActionEnumerate, makeval(info.This()), prv, NULL, NULL, &flags);
  if (!res)
    return Handle<Array>();

  Handle<Value> ret = *res;
  if (flags & natusEngValFlagUnlock) {
    res->Dispose();
    res->Clear();
  }
  if (flags & natusEngValFlagFree)
    delete res;

  if (!ret->IsArray())
    return Handle<Array>();

  return Handle<Array>(Array::Cast(*ret));
}

static Handle<Value>
int_call(const Arguments& args, Handle<Value> object)
{
  HandleScope hs;

  // Get the private pointer
  natusPrivate *priv = (natusPrivate*) args.Data()->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);

  // Convert the arguments
  Handle<Array> array = Array::New(args.Length());
  for (int i = 0; i < args.Length(); i++)
    array->Set(i, args[i]);

  // Note: when called as an object,
  // This() is *not* this. Upstream bug.
  natusEngValFlags flags = natusEngValFlagNone;
  natusEngVal obj = makeval(object);
  natusEngVal ths = makeval(args.IsConstructCall() ? (Handle<Value> ) Undefined() : args.This());
  natusEngVal arg = makeval(array);
  natusEngVal res = natus_handle_call(obj, priv, ths, arg, &flags);
  if (!res)
    return ThrowException(Undefined());

  Handle<Value> ret = *res;
  if (flags & natusEngValFlagUnlock) {
    res->Dispose();
    res->Clear();
  }
  if (flags & natusEngValFlagFree)
    delete res;

  if (flags & natusEngValFlagException)
    return ThrowException(ret);

  return ret;
}

static Handle<Value>
obj_call(const Arguments& args)
{
  return int_call(args, args.Holder());
}

static Handle<Value>
fnc_call(const Arguments& args)
{
  return int_call(args, args.Callee());
}

static void
v8_ctx_free(natusEngCtx ctx)
{
#define FORCE_GC() while(!V8::IdleNotification()) {}
  HandleScope hs;

  FORCE_GC();
  natusPrivate *priv = (natusPrivate*) (*ctx)->Global()
      ->GetHiddenValue(V8_PRIV_STRING)
      ->ToObject()
      ->GetPointerFromInternalField(V8_PRIV_SLOT);
  natus_private_free(priv);
  FORCE_GC();

  ctx->Dispose();
  ctx->Clear();
  delete ctx;
  FORCE_GC();
}

static void
v8_val_unlock(natusEngCtx ctx, natusEngVal val)
{

}

static natusEngVal
v8_val_duplicate(natusEngCtx ctx, natusEngVal val)
{
  return makeval(*val);
}

static void
v8_val_free(natusEngVal val)
{
  val->Dispose();
  val->Clear();
  V8::AdjustAmountOfExternalAllocatedMemory(((int) sizeof(Persistent<Context> )) * -1);
  delete val;
}

static natusEngVal
v8_new_global(natusEngCtx ctx, natusEngVal val, natusPrivate *priv, natusEngCtx *newctx, natusEngValFlags *flags)
{
  HandleScope hs;

  Persistent<Context> context = Context::New();
  Context::Scope cs(context);

  Handle<ObjectTemplate> ot = ObjectTemplate::New();
  ot->SetInternalFieldCount(V8_PRIV_SLOT + 1);

  Handle<Object> prv = ot->NewInstance();
  prv->SetPointerInInternalField(V8_PRIV_SLOT, priv);

  Handle<Object> global = context->Global();
  global->SetHiddenValue(V8_PRIV_STRING, prv);

  if (ctx)
    context->SetSecurityToken((*ctx)->GetSecurityToken());

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Persistent<Context> ));
  *newctx = new Persistent<Context>();
  **newctx = context;
  return makeval(global);
}

static natusEngVal
v8_new_bool(const natusEngCtx ctx, bool b, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(Boolean::New(b));
}

static natusEngVal
v8_new_number(const natusEngCtx ctx, double n, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(Number::New(n));
}

static natusEngVal
v8_new_string_utf8(const natusEngCtx ctx, const char *str, size_t len, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(String::New(str, len));
}

static natusEngVal
v8_new_string_utf16(const natusEngCtx ctx, const natusChar *str, size_t len, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(String::New(str, len));
}

static natusEngVal
v8_new_array(const natusEngCtx ctx, const natusEngVal *array, size_t len, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  Handle<Array> valv = Array::New(len);
  for (unsigned int i = 0; i < len; i++)
    valv->Set(i, *(array[i]));
  return makeval(valv);
}

static natusEngVal
v8_new_function(const natusEngCtx ctx, const char *name, natusPrivate *priv, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  Handle<FunctionTemplate> ft;
  Handle<ObjectTemplate> ot;
  Handle<Function> fnc;
  Handle<Value> prv;

  TryCatch tc;
  ot = ObjectTemplate::New();
  if (tc.HasCaught())
    goto exception;

  ot->SetInternalFieldCount(V8_PRIV_SLOT + 1);
  if (tc.HasCaught())
    goto exception;

  prv = ot->NewInstance();
  if (tc.HasCaught())
    goto exception;

  prv->ToObject()->SetPointerInInternalField(V8_PRIV_SLOT, priv);
  if (tc.HasCaught())
    goto exception;

  ft = FunctionTemplate::New(fnc_call, prv);
  if (tc.HasCaught())
    goto exception;

  if (name) {
    ft->SetClassName(String::New(name));
    if (tc.HasCaught())
      goto exception;
  }

  fnc = ft->GetFunction();
  if (tc.HasCaught())
    goto exception;

  fnc->SetHiddenValue(V8_PRIV_STRING, prv);
  if (tc.HasCaught())
    goto exception;

  Persistent<Value>::New(prv).MakeWeak(priv, on_free);
  return makeval(fnc);

exception:
  natus_private_free(priv);
  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_new_object(const natusEngCtx ctx, natusClass *cls, natusPrivate *priv, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  Handle<ObjectTemplate> ot;
  Handle<Object> obj;
  Handle<Object> prv;

  TryCatch tc;
  ot = ObjectTemplate::New();
  if (tc.HasCaught())
    goto exception;

  ot->SetInternalFieldCount(V8_PRIV_SLOT + 1);
  if (tc.HasCaught())
    goto exception;

  prv = ot->NewInstance();
  if (tc.HasCaught())
    goto exception;

  prv->SetPointerInInternalField(V8_PRIV_SLOT, priv);
  if (tc.HasCaught())
    goto exception;

  ot = ObjectTemplate::New();
  if (tc.HasCaught())
    goto exception;

  if (cls) {
    if (cls->get || cls->set || cls->del || cls->enumerate) {
      ot->SetNamedPropertyHandler(cls->get ? obj_property_get : NULL,
      cls->set ? obj_property_set : NULL, NULL,
      cls->del ? obj_property_del : NULL,
      cls->enumerate ? obj_enumerate : NULL, prv);
      ot->SetIndexedPropertyHandler(cls->get ? obj_item_get : NULL,
      cls->set ? obj_item_set : NULL, NULL,
      cls->del ? obj_item_del : NULL,
      cls->enumerate ? obj_enumerate : NULL, prv);
    }

    if (cls->call)
      ot->SetCallAsFunctionHandler(obj_call, prv);
  }

  obj = ot->NewInstance();
  if (tc.HasCaught())
    goto exception;

  obj->SetHiddenValue(V8_PRIV_STRING, prv);
  if (tc.HasCaught())
    goto exception;

  Persistent<Value>::New(prv).MakeWeak(priv, on_free);
  return makeval(obj);

exception:
  natus_private_free(priv);
  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_new_null(const natusEngCtx ctx, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(Null());
}

static natusEngVal
v8_new_undefined(const natusEngCtx ctx, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval(Undefined());
}

static bool
v8_to_bool(const natusEngCtx ctx, const natusEngVal val)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return (*val)->BooleanValue();
}

static double
v8_to_double(const natusEngCtx ctx, const natusEngVal val)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return (*val)->NumberValue();
}

static char *
v8_to_string_utf8(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  TryCatch tc;
  Handle<String> str = (*val)->ToString();
  if (tc.HasCaught()) {
    assert((*val)->IsObject());
    str = String::New("[object NativeObject]");
  }

  *len = str->Utf8Length();

  char* buff = (char*) malloc(sizeof(char) * (*len + 1));
  if (!buff)
    return NULL;
  memset(buff, 0, sizeof(char) * (*len + 1));

  str->WriteUtf8(buff);
  return buff;
}

static natusChar *
v8_to_string_utf16(const natusEngCtx ctx, const natusEngVal val, size_t *len)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  TryCatch tc;
  Handle<String> str = (*val)->ToString();
  if (tc.HasCaught()) {
    assert((*val)->IsObject());
    str = String::New("[object NativeObject]");
  }

  *len = str->Length();

  natusChar* buff = (natusChar*) malloc(sizeof(natusChar) * (*len + 1));
  if (!buff)
    return NULL;
  memset(buff, 0, sizeof(natusChar) * (*len + 1));

  str->Write(buff);
  return buff;
}

static natusEngVal
v8_del(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  TryCatch tc;
  bool rslt;

  if ((*id)->IsNumber() || (*id)->IsInt32() || (*id)->IsUint32())
    rslt = (*val)->ToObject()->Delete((*id)->ToInteger()->Int32Value());
  else
    rslt = (*val)->ToObject()->Delete((*id)->ToString());

  if (!tc.HasCaught())
    return makeval(Boolean::New(rslt));

  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_get(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  TryCatch tc;
  Handle<Value> rslt = (*val)->ToObject()->Get(*id);
  if (!tc.HasCaught())
    return makeval(rslt);

  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_set(const natusEngCtx ctx, natusEngVal val, const natusEngVal id, const natusEngVal value, natusPropAttr attrs, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  TryCatch tc;
  bool rslt = (*val)->ToObject()->Set(*id, *value, (PropertyAttribute) attrs);
  if (!tc.HasCaught())
    return makeval(Boolean::New(rslt));

  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_enumerate(const natusEngCtx ctx, natusEngVal val, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  return makeval((*val)->ToObject()->GetPropertyNames());
}

static natusEngVal
v8_call(const natusEngCtx ctx, natusEngVal func, natusEngVal ths, natusEngVal args, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  assert((*func)->IsFunction());

  // Convert arguments
  uint32_t len = (*args)->ToObject()->Get(String::New("length"))->ToArrayIndex()->Uint32Value();
  Handle<Value> argv[len];
  for (uint32_t i = 0; i < len; i++)
    argv[i] = (*args)->ToObject()->Get(i);

  // Call it
  TryCatch tc;
  Handle<Value> res;
  if ((*ths)->IsUndefined())
    res = Function::Cast(**func)->NewInstance(len, argv);
  else
    res = Function::Cast(**func)->Call((*ths)->ToObject(), len, argv);

  if (!tc.HasCaught())
    return makeval(res);

  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusEngVal
v8_evaluate(const natusEngCtx ctx, natusEngVal ths, const natusEngVal jscript, const natusEngVal filename, unsigned int lineno, natusEngValFlags *flags)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  // Execute the script
  TryCatch tc;
  ScriptOrigin so = ScriptOrigin(*filename, Integer::New(lineno));
  Handle<Script> script = Script::Compile((*jscript)->ToString(), &so);
  Handle<Value> res = script->Run();
  if (!tc.HasCaught())
    return makeval(res);

  *flags = (natusEngValFlags) (*flags | natusEngValFlagException);
  return makeval(tc.Exception());
}

static natusPrivate *
v8_get_private(const natusEngCtx ctx, const natusEngVal val)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  Handle<Value> hidden = (*val)->ToObject()->GetHiddenValue(V8_PRIV_STRING);
  if (hidden.IsEmpty() || !hidden->IsObject())
    return NULL;
  return (natusPrivate*) hidden->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);
}

static natusEngVal
v8_get_global(const natusEngCtx ctx, const natusEngVal val)
{
  HandleScope hs;
  Context::Scope cs(*ctx);
  return makeval((*ctx)->Global());
}

static natusValueType
v8_get_type(const natusEngCtx ctx, const natusEngVal val)
{
  if ((*val)->IsArray())
    return natusValueTypeArray;
  else if ((*val)->IsBoolean())
    return natusValueTypeBoolean;
  else if ((*val)->IsFunction())
    return natusValueTypeFunction;
  else if ((*val)->IsNull())
    return natusValueTypeNull;
  else if ((*val)->IsNumber() || (*val)->IsInt32() || (*val)->IsUint32())
    return natusValueTypeNumber;
  else if ((*val)->IsObject() || (*val)->IsDate() /*|| (*val)->IsRegExp()*/)
    return natusValueTypeObject;
  else if ((*val)->IsString())
    return natusValueTypeString;
  else if ((*val)->IsUndefined())
    return natusValueTypeUndefined;
  else
    return natusValueTypeUnknown;
}

static bool
v8_borrow_context(natusEngCtx ctx, natusEngVal val, void **context, void **value)
{
  *context = ctx;
  *value = val;
  return true;
}

static bool
v8_equal(const natusEngCtx ctx, const natusEngVal val1, const natusEngVal val2, bool strict)
{
  HandleScope hs;
  Context::Scope cs(*ctx);

  if (strict)
    return (*val1)->StrictEquals(*val2);
  return (*val1)->Equals(*val2);
}

__attribute__((constructor))
static void
_init()
{
  // Make sure that v8 doesn't change its enum values
  assert(None == (PropertyAttribute) natusPropAttrNone);
  assert(ReadOnly == (PropertyAttribute) natusPropAttrReadOnly);
  assert(DontEnum == (PropertyAttribute) natusPropAttrDontEnum);
  assert(DontDelete == (PropertyAttribute) natusPropAttrDontDelete);
}

// V8_Fatal appears to be the only v8 unique extern "C" symbol
// Let's hope it doesn't get removed...
NATUS_ENGINE("v8", "V8_Fatal", v8);
