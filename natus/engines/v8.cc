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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/backend.h>

#include <v8.h>
using namespace v8;

#define CTX(v) (((v8Value*) v)->ctx)
#define VAL(v) (((v8Value*) v)->val)
#define OBJ(v) (JSValueToObject(CTX(v), VAL(v), NULL))
#define ENG(v) ((v8Engine*) v->eng->engine)
#define V8_PRIV_STRING String::New("__private__")
#define V8_PRIV_SLOT 0

struct v8Private {
	static void private_free(Persistent<Value> object, void* priv) {
		object.Dispose();
		object.Clear();
		nt_private_free(((v8Private*) priv)->priv);
		delete (v8Private*) priv;
	}
	Persistent<Object> hndl;
	ntPrivate*         priv;
};

static inline ntPrivate* private_get(Handle<Value> val) {
	HandleScope hs;

	if (!val->IsFunction() && !val->IsObject()) return NULL;
	Handle<Value> hidden = val->ToObject()->GetHiddenValue(V8_PRIV_STRING);
	if (hidden.IsEmpty() || !hidden->IsObject()) return NULL;
	return (ntPrivate*) hidden->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);
}

struct v8Value : public ntValue {
	Persistent<Context> ctx;
	Persistent<Value>   val;

	static ntValue* getInstance(Handle<Context> ctx, ntPrivate* priv) {
		HandleScope hs;
		Context::Scope cs(ctx);

		// Make the object which holds our private pointer
		Handle<ObjectTemplate> prvt = ObjectTemplate::New();
		prvt->SetInternalFieldCount(V8_PRIV_SLOT+1);
		Handle<Object> prv = prvt->NewInstance();
		prv->SetPointerInInternalField(V8_PRIV_SLOT, priv);
		v8Private* tmp = new v8Private;
		tmp->hndl = *prv;
		tmp->hndl.MakeWeak(tmp, v8Private::private_free);

		// Create the v8Value and store the private
		v8Value *self = new v8Value(ctx, ctx->Global());
		VAL(self)->ToObject()->SetHiddenValue(V8_PRIV_STRING, prv);

		V8::AdjustAmountOfExternalAllocatedMemory(sizeof(v8Value));
		return self;
	}

	static ntValue* getInstance(const ntValue* ctx, Handle<Value> val, bool exc=false) {
		ntValue* global = (ntValue*) nt_private_get(private_get(val), NATUS_PRIV_GLOBAL);
		if (global && VAL(global) == val) return nt_value_incref(global);

		v8Value* self = new v8Value(CTX(ctx), val);
		self->ref = 1;
		self->exc = exc;
		self->eng = nt_engine_incref(ctx->eng);
		self->typ = ntValueTypeUnknown;
		V8::AdjustAmountOfExternalAllocatedMemory(sizeof(v8Value));
		return self;
	}

	v8Value(Handle<Context> ctx, Handle<Value> val) {
		this->ctx = Persistent<Context>::New(ctx);
		this->val = Persistent<Value>::New(val);
	}

	virtual ~v8Value() {
		val.Dispose();
		val.Clear();
		ctx.Dispose();
		ctx.Clear();
	}
};

static inline Handle<Value> property_handler(uint32_t index, Handle<String> property, Handle<Value> value, const AccessorInfo& info, bool del=false) {
	HandleScope hs;

	ntPrivate* prv = (ntPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);
	ntClass*   cls = (ntClass*) nt_private_get(prv, NATUS_PRIV_CLASS);
	ntValue*   glb = (ntValue*) nt_private_get(prv, NATUS_PRIV_GLOBAL);
	assert(cls && glb);

	ntValue*   obj = v8Value::getInstance(glb, info.This());
	ntValue*   idx = property.IsEmpty() ? nt_value_new_number(glb, index) : v8Value::getInstance(glb, property);
	ntValue*   val = value.IsEmpty() ? NULL : v8Value::getInstance(glb, value);
	ntValue*   res = NULL;

	if (!val) {
		if (del) {
			assert(cls->del);
			res = cls->del(cls, obj, idx);
		} else {
			assert(cls->get);
			res = cls->get(cls, obj, idx);
		}
	} else {
		assert(cls->set);
		res = cls->set(cls, obj, idx, val);
	}
	nt_value_decref(obj);
	nt_value_decref(idx);
	nt_value_decref(val);

	if (nt_value_is_undefined(res)) {
		nt_value_decref(res);
		return Handle<Value>();
	}

	if (nt_value_is_exception(res))
		return ThrowException(VAL(res));

	if (value.IsEmpty() && !del) {
		Handle<Value> r = VAL(res);
		nt_value_decref(res);
		return r;
	}

	return Boolean::New(true);
}

static Handle<Value> obj_item_get(uint32_t index, const AccessorInfo& info) {
	return property_handler(index, Handle<String>(), Handle<Value>(), info);
}

static Handle<Value> obj_property_get(Local<String> property, const AccessorInfo& info) {
	return property_handler(0, property, Handle<Value>(), info);
}

static Handle<Boolean> obj_item_del(uint32_t index, const AccessorInfo& info) {
	Handle<Value> v = property_handler(index, Handle<String>(), Handle<Value>(), info, true);
	if (v.IsEmpty()) return Handle<Boolean>();
	return v->ToBoolean();
}

static Handle<Boolean> obj_property_del(Local<String> property, const AccessorInfo& info) {
	Handle<Value> v = property_handler(0, property, Handle<Value>(), info, true);
	if (v.IsEmpty()) return Handle<Boolean>();
	return v->ToBoolean();
}

static Handle<Value> obj_item_set(uint32_t index, Local<Value> value, const AccessorInfo& info) {
	return property_handler(index, Handle<String>(), value, info);
}

static Handle<Value> obj_property_set(Local<String> property, Local<Value> value, const AccessorInfo& info) {
	return property_handler(0, property, value, info);
}

static Handle<Array> obj_enumerate(const AccessorInfo& info) {
	HandleScope hs;

	ntPrivate* prv = (ntPrivate*) info.Data()->ToObject()->GetPointerFromInternalField(V8_PRIV_SLOT);
	ntClass*   cls = (ntClass*) nt_private_get(prv, NATUS_PRIV_CLASS);
	ntValue*   glb = (ntValue*) nt_private_get(prv, NATUS_PRIV_GLOBAL);
	assert(glb && cls && cls->enumerate);

	ntValue*   obj = v8Value::getInstance(glb, info.This());
	ntValue*   val = cls->enumerate(cls, obj);
	nt_value_decref(obj);
	if (!val || !nt_value_is_array(val))
		return Handle<Array>();
	return Handle<Array>(Array::Cast(*VAL(val)));
}

static Handle<Value> int_call(const Arguments& args, ntValue* glbl, ntNativeFunction func, ntClass* clss) {
	HandleScope hs;
	Context::Scope cs(CTX(glbl));

	// Convert the arguments
	Handle<Array> array = Array::New(args.Length());
	for (int i=0 ; i < args.Length() ; i++)
		array->Set(i, args[i]);
	Handle<Value> function;

	// For objects, the object is stored in Holder
	if (func) function = args.Callee();
	else      function = args.Holder();

	ntValue* fnc = v8Value::getInstance(glbl, function);
	ntValue* ths = NULL;
	ntValue* arg = v8Value::getInstance(glbl, array);
	ntValue *res = NULL;

	// Note: when called as an object,
	// This() is *not* this. Upstream bug.
	if (!args.IsConstructCall())
		ths = v8Value::getInstance(glbl, args.This());
	else
		ths = v8Value::getInstance(glbl, Undefined());

	if (func)
		res = func(fnc, ths, arg);
	else
		res = clss->call(clss, fnc, ths, arg);

	Handle<Value> r = Undefined();
	if (res) r = VAL(res);
	bool exc = !res || res->exc;
	nt_value_decref(fnc);
	nt_value_decref(ths);
	nt_value_decref(arg);
	nt_value_decref(res);

	return exc ? ThrowException(r) : r;
}

static Handle<Value> obj_call(const Arguments& args) {
	ntPrivate* priv = private_get(args.Holder());
	ntClass*   clss = (ntClass*) nt_private_get(priv, NATUS_PRIV_CLASS);
	ntValue*   glbl = (ntValue*) nt_private_get(priv, NATUS_PRIV_GLOBAL);
	assert(glbl && clss && clss->call);
	return int_call(args, glbl, NULL, clss);
}

static Handle<Value> fnc_call(const Arguments& args) {
	ntPrivate*       priv = private_get(args.Callee());
	ntNativeFunction func = (ntNativeFunction) nt_private_get(priv, NATUS_PRIV_FUNCTION);
	ntValue*         glbl = (ntValue*) nt_private_get(priv, NATUS_PRIV_GLOBAL);
	assert(glbl && func);
	return int_call(args, glbl, func, NULL);
}

static ntPrivate       *v8_value_private_get      (const ntValue *val) {
	return private_get(VAL(val));
}

static bool             v8_value_borrow_context   (const ntValue *ctx, void **context, void **value) {
	if (context) *context = &CTX(ctx);
	if (value)   *value   = &VAL(ctx);
	return true;
}

static ntValue*         v8_value_get_global       (const ntValue *val) {
	HandleScope hs;
	return (ntValue*) nt_private_get(private_get(CTX(val)->Global()), NATUS_PRIV_GLOBAL);
}

static ntValueType      v8_value_get_type         (const ntValue *val) {
	if (VAL(val)->IsArray())
		return ntValueTypeArray;
	else if (VAL(val)->IsBoolean())
		return ntValueTypeBool;
	else if (VAL(val)->IsFunction())
		return ntValueTypeFunction;
	else if (VAL(val)->IsNull())
		return ntValueTypeNull;
	else if (VAL(val)->IsNumber() || VAL(val)->IsInt32() || VAL(val)->IsUint32())
		return ntValueTypeNumber;
	else if (VAL(val)->IsObject() || VAL(val)->IsDate() /*|| val->IsRegExp()*/)
		return ntValueTypeObject;
	else if (VAL(val)->IsString())
		return ntValueTypeString;
	else if (VAL(val)->IsUndefined())
		return ntValueTypeUndefined;
	else
		return ntValueTypeUnknown;
}

static void             v8_value_free             (ntValue *val) {
	nt_engine_decref(val->eng);
	delete (v8Value*) val;
	V8::AdjustAmountOfExternalAllocatedMemory(((ssize_t) sizeof(v8Value)) * -1);
}

static ntValue*         v8_value_new_bool         (const ntValue *ctx, bool b) {
	HandleScope hs;
	return v8Value::getInstance(ctx, Boolean::New(b));
}

static ntValue*         v8_value_new_number       (const ntValue *ctx, double n) {
	HandleScope hs;
	return v8Value::getInstance(ctx, Number::New(n));
}

static ntValue*         v8_value_new_string_utf8  (const ntValue *ctx, const char   *str, size_t len) {
	HandleScope hs;
	Handle<String> s = String::New(str, len);
	ntValue* v = v8Value::getInstance(ctx, s);
	return v;
}

static ntValue*         v8_value_new_string_utf16 (const ntValue *ctx, const ntChar *str, size_t len) {
	HandleScope hs;
	return v8Value::getInstance(ctx, String::New(str, len));
}

static ntValue*         v8_value_new_array        (const ntValue *ctx, const ntValue **array) {
	HandleScope hs;
	Context::Scope cs(CTX(ctx));

	int len = 0;
	for( ; array[len] ; len++);

	Handle<Array> valv = Array::New(len);
	for (unsigned int i=0 ; array[i] ; i++)
		valv->Set(i, VAL(array[i]));
	return v8Value::getInstance(ctx, valv);
}

static ntValue*         v8_value_new_function     (const ntValue *ctx, ntPrivate *priv) {
	HandleScope hs;
	Context::Scope cs(CTX(ctx));

	// Make the object which holds our private pointer
	Handle<ObjectTemplate> prvt = ObjectTemplate::New();
	prvt->SetInternalFieldCount(V8_PRIV_SLOT+1);
	Handle<Object> prv = prvt->NewInstance();
	prv->SetPointerInInternalField(V8_PRIV_SLOT, priv);
	v8Private* tmp = new v8Private;
	tmp->hndl = *prv;
	tmp->hndl.MakeWeak(tmp, v8Private::private_free);

	Handle<FunctionTemplate> ft = FunctionTemplate::New(fnc_call, prv);
	Handle<Function>        fnc = ft->GetFunction();
	fnc->SetHiddenValue(V8_PRIV_STRING, prv);
	return v8Value::getInstance(ctx, fnc);
}

static ntValue*         v8_value_new_object       (const ntValue *ctx, ntClass *cls, ntPrivate *priv) {
	HandleScope hs;
	Context::Scope cs(CTX(ctx));

	// Make the object which holds our private pointer
	Handle<ObjectTemplate> prvt = ObjectTemplate::New();
	prvt->SetInternalFieldCount(V8_PRIV_SLOT+1);
	Handle<Object> prv = prvt->NewInstance();
	prv->SetPointerInInternalField(V8_PRIV_SLOT, priv);
	v8Private* tmp = new v8Private;
	tmp->hndl = *prv;
	tmp->hndl.MakeWeak(tmp, v8Private::private_free);

	Handle<ObjectTemplate> ot = ObjectTemplate::New();
	if (cls) {
		if (cls->get || cls->set || cls->del || cls->enumerate) {
			ot->SetNamedPropertyHandler(cls->get       ? obj_property_get : NULL,
					                    cls->set       ? obj_property_set : NULL, NULL,
										cls->del       ? obj_property_del : NULL,
										cls->enumerate ? obj_enumerate    : NULL, prv);
			ot->SetIndexedPropertyHandler(cls->get       ? obj_item_get   : NULL,
                                          cls->set       ? obj_item_set   : NULL, NULL,
										  cls->del       ? obj_item_del   : NULL,
										  cls->enumerate ? obj_enumerate  : NULL, prv);
		}

		if (cls->call)
			ot->SetCallAsFunctionHandler(obj_call, prv);
	}

	Handle<Object> obj = ot->NewInstance();
	obj->SetHiddenValue(V8_PRIV_STRING, prv);

	return v8Value::getInstance(ctx, obj);
}

static ntValue*         v8_value_new_null         (const ntValue *ctx) {
	HandleScope hs;

	return v8Value::getInstance(ctx, Null());
}

static ntValue*         v8_value_new_undefined    (const ntValue *ctx) {
	HandleScope hs;

	return v8Value::getInstance(ctx, Undefined());
}

static bool             v8_value_to_bool          (const ntValue *val) {
	return VAL(val)->BooleanValue();
}

static double           v8_value_to_double        (const ntValue *val) {
	return VAL(val)->NumberValue();
}

static Handle<String> _val_to_string(Handle<Context> ctx, Handle<Value> val) {
	HandleScope hs;
	Context::Scope cs(ctx);
	TryCatch tc;

	Handle<Object> oval = val->ToObject();
	if (!tc.HasCaught()) {
		Handle<Value> toString = oval->Get(String::New("toString"));
		if (!toString.IsEmpty() && toString->IsFunction()) {
			Handle<Value> rslt = Function::Cast(*toString)->Call(val->ToObject(), 0, NULL);
			if (rslt->IsString()) return rslt->ToString();
		}
	}
	tc.Reset();

	Handle<String> str = val->ToString();
	if (tc.HasCaught()) {
		assert(val->IsObject());
		str = String::New("[object NativeObject]");
	}

	return str;
}

static char            *v8_value_to_string_utf8   (const ntValue *val, size_t *len) {
	HandleScope hs;
	Context::Scope cs(CTX(val));

	Handle<String> str = _val_to_string(CTX(val), VAL(val));

	*len = str->Utf8Length();

	char* buff = (char*) malloc(sizeof(char) * (*len + 1));
	if (!buff) return NULL;
	memset(buff, 0, sizeof(char) * (*len + 1));

	str->WriteUtf8(buff);
	return buff;
}

static ntChar          *v8_value_to_string_utf16  (const ntValue *val, size_t *len) {
	HandleScope hs;
	Context::Scope cs(CTX(val));

	Handle<String> str = _val_to_string(CTX(val), VAL(val));

	*len = str->Length();

	ntChar* buff = (ntChar*) malloc(sizeof(ntChar) * (*len + 1));
	if (!buff) return NULL;
	memset(buff, 0, sizeof(ntChar) * (*len + 1));

	str->Write(buff);
	return buff;
}

static ntValue         *v8_value_del              (ntValue *obj, const ntValue *id) {
	HandleScope hs;
	Context::Scope cs(CTX(obj));

	TryCatch tc;
	bool rslt;

	if (nt_value_is_number(id))
		rslt = VAL(obj)->ToObject()->Delete(VAL(id)->ToInteger()->Int32Value());
	else
		rslt = VAL(obj)->ToObject()->Delete(VAL(id)->ToString());

	if (tc.HasCaught())
		return v8Value::getInstance(obj, tc.Exception(), true);
	return v8Value::getInstance(obj, Boolean::New(rslt));
}

static ntValue         *v8_value_get              (const ntValue *obj, const ntValue *id) {
	HandleScope hs;
	Context::Scope cs(CTX(obj));

	TryCatch tc;
	Handle<Value> rslt = VAL(obj)->ToObject()->Get(VAL(id));
	if (tc.HasCaught())
		return v8Value::getInstance(obj, tc.Exception(), true);
	return v8Value::getInstance(obj, rslt, false);
}

static ntValue         *v8_value_set              (ntValue *obj, const ntValue *id, const ntValue *value, ntPropAttr attrs) {
	HandleScope hs;
	Context::Scope cs(CTX(obj));

	TryCatch tc;
	bool rslt = VAL(obj)->ToObject()->Set(VAL(id), VAL(value), (PropertyAttribute) attrs);
	if (tc.HasCaught())
		return v8Value::getInstance(obj, tc.Exception(), true);
	return v8Value::getInstance(obj, Boolean::New(rslt), false);
}

static ntValue         *v8_value_enumerate        (const ntValue *obj) {
	HandleScope hs;
	Context::Scope cs(CTX(obj));

	return v8Value::getInstance(obj, VAL(obj)->ToObject()->GetPropertyNames());
}

static ntValue         *v8_value_call             (ntValue *func, ntValue *ths, ntValue *args) {
	HandleScope hs;
	Context::Scope cs(CTX(func));

	// Convert arguments
	size_t len = nt_value_as_long(nt_value_get_utf8(args, "length"));
	Handle<Value>* argv = new Handle<Value>[len];
	for (unsigned int i=0 ; i < len ; i++) {
		ntValue* item = nt_value_get_index(args, i);
		if (!item) {
			len = i;
			break;
		}
		argv[i] = VAL(item);
		nt_value_decref(item);
	}

	assert(VAL(func)->IsFunction());

	// Call it
	TryCatch tc;
	Handle<Value> res;
	if (nt_value_is_undefined(ths))
		res = Function::Cast(*VAL(func))->NewInstance(len, argv);
	else
		res = Function::Cast(*VAL(func))->Call(VAL(ths)->ToObject(), len, argv);
	delete[] argv;

	return v8Value::getInstance(func, tc.HasCaught() ? tc.Exception() : res, tc.HasCaught());
}

static ntValue         *v8_value_evaluate         (ntValue *ths, const ntValue *jscript, const ntValue *filename, unsigned int lineno) {
	HandleScope hs;
	Context::Scope cs(CTX(ths));

	// Execute the script
	TryCatch tc;
	ScriptOrigin       so = ScriptOrigin(VAL(filename), Integer::New(lineno));
	Handle<Script> script = Script::Compile(VAL(jscript)->ToString(), &so);
	Handle<Value>     res = script->Run();
	if (tc.HasCaught()) res = tc.Exception();

	return v8Value::getInstance(ths, res, tc.HasCaught());
}

static void v8_engine_free(void *engine) {

}

static ntValue* v8_engine_newg(void *engine, ntValue* global) {
	HandleScope hs;

	Handle<Context> ctx = Context::New();
	if (global) ctx->SetSecurityToken(CTX(global)->GetSecurityToken());
	return v8Value::getInstance(ctx, nt_private_init());
}

static void* v8_engine_init() {
	// Make sure that v8 doesn't change its enum values
	assert(None       == (PropertyAttribute) ntPropAttrNone);
	assert(ReadOnly   == (PropertyAttribute) ntPropAttrReadOnly);
	assert(DontEnum   == (PropertyAttribute) ntPropAttrDontEnum);
	assert(DontDelete == (PropertyAttribute) ntPropAttrDontDelete);
	return (void*) 0x1234;
}

// V8_Fatal appears to be the only v8 unique extern "C" symbol
// Let's hope it doesn't get removed...
NATUS_ENGINE("v8", "V8_Fatal", v8);
