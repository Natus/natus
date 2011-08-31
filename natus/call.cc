#include <natus-internal.hh>

Value
Value::call(Value ths, va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return call(ths, array);
}

Value
Value::call(Value ths, Value args)
{
  return natus_call_array(internal, ths.internal, args.internal);
}

Value
Value::call(Value ths, const Value *arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = call(ths, new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}

Value
Value::call(UTF8 name, va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return call(name, array);
}

Value
Value::call(UTF8 name, Value args)
{
  Value func = get(name);
  return natus_call_array(func.internal, internal, args.internal);
}

Value
Value::call(UTF8 name, const Value *arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = call(name, new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}

Value
Value::call(UTF16 name, va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return call(name, array);
}

Value
Value::call(UTF16 name, Value args)
{
  Value func = get(name);
  return natus_call_array(func.internal, internal, args.internal);
}

Value
Value::call(UTF16 name, const Value *arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = call(name, new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}

Value
Value::callNew(va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return callNew(array);
}

Value
Value::callNew(Value args)
{
  return natus_call_new_array(internal, args.internal);
}

Value
Value::callNew(const Value* arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = callNew(new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}

Value
Value::callNew(UTF8 name, va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return callNew(name, array);
}

Value
Value::callNew(UTF8 name, Value args)
{
  Value fnc = get(name);
  return natus_call_new_array(fnc.internal, args.internal);
}

Value
Value::callNew(UTF8 name, const Value *arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = callNew(name, new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}

Value
Value::callNew(UTF16 name, va_list ap)
{
  Value array = newArray(ap);
  if (array.isException())
    return array;
  return callNew(name, array);
}

Value
Value::callNew(UTF16 name, Value args)
{
  Value fnc = get(name);
  return natus_call_new_array(fnc.internal, args.internal);
}

Value
Value::callNew(UTF16 name, const Value *arg0, ...)
{
  va_list ap;
  va_start(ap, arg0);
  Value ret = callNew(name, new_array(*this, arg0, ap));
  va_end(ap);
  return ret;
}
