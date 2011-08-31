#include <natus-internal.hh>

#include <cassert>

static natusValue *
txFunction(natusValue *obj, natusValue *ths, natusValue *args)
{
  Value o = natus_incref(obj);
  Value t = natus_incref(ths);
  Value a = natus_incref(args);

  NativeFunction f = o.getPrivate(NativeFunction);
  if (!f)
    return NULL;

  Value rslt = f(o, t, a);

  return natus_incref(rslt.borrowCValue());
}

struct txClass {
  natusClass hdr;
  Class *cls;

  static natusValue *
  del(natusClass *cls, natusValue *obj, const natusValue *prop)
  {
    Value o(obj, false);
    Value n((natusValue*) prop, false);

    Class *txcls = ((txClass *) cls)->cls;
    return natus_incref(txcls->del(o, n).borrowCValue());
  }

  static natusValue *
  get(natusClass *cls, natusValue *obj, const natusValue *prop)
  {
    Value o(obj, false);
    Value n((natusValue*) prop, false);

    Class *txcls = ((txClass *) cls)->cls;
    return natus_incref(txcls->get(o, n).borrowCValue());
  }

  static natusValue *
  set(natusClass *cls, natusValue *obj, const natusValue *prop, const natusValue *value)
  {
    Value o(obj, false);
    Value n((natusValue*) prop, false);
    Value v((natusValue*) value, false);

    Class *txcls = ((txClass *) cls)->cls;
    return natus_incref(txcls->set(o, n, v).borrowCValue());
  }

  static natusValue *
  enumerate(natusClass *cls, natusValue *obj)
  {
    Value o(obj, false);

    Class *txcls = ((txClass *) cls)->cls;
    return natus_incref(txcls->enumerate(o).borrowCValue());
  }

  static natusValue *
  call(natusClass *cls, natusValue *obj, natusValue *ths, natusValue* args)
  {
    Value o(obj, false);
    Value t(ths, false);
    Value a(args, false);

    Class *txcls = ((txClass *) cls)->cls;
    Value rslt = txcls->call(o, t, a);

    return natus_incref(rslt.borrowCValue());
  }

  static void
  free(natusClass *cls)
  {
    assert(cls && ((txClass *) cls)->cls);
    Class *txcls = ((txClass *) cls)->cls;
    txcls->free();
    delete cls;
  }

  txClass(Class* cls)
  {
    assert(cls);

    Class::Hooks hooks = cls->getHooks();
    this->hdr.del = (hooks & Class::HookDel) ? txClass::del : NULL;
    this->hdr.get = (hooks & Class::HookGet) ? txClass::get : NULL;
    this->hdr.set = (hooks & Class::HookSet) ? txClass::set : NULL;
    this->hdr.enumerate = (hooks & Class::HookEnumerate) ? txClass::enumerate : NULL;
    this->hdr.call = (hooks & Class::HookCall) ? txClass::call : NULL;
    this->hdr.free = txClass::free;
    this->cls = cls;
  }
};

Value
new_array(const Value& ctx, const Value *arg0, va_list ap)
{
  va_list apc;
  size_t count = 1;

  if (!arg0)
    return ctx.newArray();

  va_copy(apc, ap);
  while (va_arg(apc, const Value*))
    count++;
  va_end(apc);

  const Value* array[count + 1];
  array[0] = arg0;
  va_copy(apc, ap);
  for (size_t i=1; i < count; i++)
    array[i] = va_arg(apc, const Value*);
  va_end(apc);

  return ctx.newArray(array);
}

Value
Value::newBoolean(bool b) const
{
  return natus_new_boolean(internal, b);
}

Value
Value::newNumber(double n) const
{
  return natus_new_number(internal, n);
}

Value
Value::newString(UTF8 string) const
{
  return natus_new_string_utf8_length(internal, string.data(), string.length());
}

Value
Value::newString(UTF16 string) const
{
  return natus_new_string_utf16_length(internal, string.data(), string.length());
}

Value
Value::newString(const char* fmt, va_list arg)
{
  return natus_new_string_varg(internal, fmt, arg);
}

Value
Value::newString(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  natusValue *tmpv = natus_new_string_varg(internal, fmt, ap);
  va_end(ap);
  return tmpv;
}

Value
Value::newArray(const Value* const * array) const
{
  long len;
  for (len = 0; array && array[len]; len++)
    ;

  const natusValue* a[len + 1];
  for(len = 0; array && array[len]; len++)
  a[len] = array[len]->borrowCValue ();
  a[len] = NULL;

  return natus_new_array_vector(internal, a);
}

Value
Value::newArray(va_list ap) const
{
  va_list apc;
  size_t count = 0;

  va_copy(apc, ap);
  while (va_arg(apc, const Value*))
    count++;
  va_end(apc);

  const Value* array[count + 1];
  va_copy(apc, ap);
  for (size_t i=0 ; i < count ; i++)
    array[i] = va_arg(apc, const Value*);
  va_end(apc);

  return newArray(array);
}

Value
Value::newArray(const Value* item, ...) const
{
  va_list ap;
  va_start(ap, item);
  Value ret = new_array(*this, item, ap);
  va_end(ap);
  return ret;
}

Value
Value::newFunction(NativeFunction func, const char* name) const
{
  Value f = natus_new_function(internal, txFunction, name);
  f.setPrivate(NativeFunction, func);
  return f;
}

Value
Value::newObject(Class* cls) const
{
  if (!cls)
    return natus_new_object(internal, NULL);
  return natus_new_object(internal, (natusClass*) new txClass(cls));
}

Value
Value::newNull() const
{
  return natus_new_null(internal);
}

Value
Value::newUndefined() const
{
  return natus_new_undefined(internal);
}
