#include <natus-internal.hh>

Class::Hooks
Class::getHooks()
{
  return Class::HookNone;
}

Value
Class::del(Value& obj, Value& idx)
{
  return NULL;
}

Value
Class::get(Value& obj, Value& idx)
{
  return NULL;
}

Value
Class::set(Value& obj, Value& idx, Value& val)
{
  return NULL;
}

Value
Class::enumerate(Value& obj)
{
  return NULL;
}

Value
Class::call(Value& obj, Value& ths, Value& arg)
{
  return NULL;
}

Value::Value()
{
  internal = NULL;
}

Value::Value(natusValue *value, bool steal)
{
  if (!steal)
    natus_incref(value);
  internal = value;
}

Value::Value(const Value& value)
{
  internal = natus_incref(value.internal);
}

Value::~Value()
{
  natus_decref(internal);
  internal = NULL;
}

Value&
Value::operator=(const Value& value)
{
  natus_decref(internal);
  internal = natus_incref(value.internal);
  return *this;
}

Value
Value::newGlobal(const Value global)
{
  return natus_new_global_shared(global.internal);
}

Value
Value::newGlobal(const char *name_or_path)
{
  return natus_new_global(name_or_path);
}

Value
Value::getGlobal() const
{
  return natus_incref(natus_get_global(internal));
}

const char*
Value::getEngineName() const
{
  return natus_get_engine_name(internal);
}

bool
Value::borrowContext(void **context, void **value) const
{
  return natus_borrow_context(internal, context, value);
}

natusValue*
Value::borrowCValue() const
{
  return internal;
}
