#include <natus-internal.hh>

bool
Value::operator==(const Value& value)
{
  return equals(value);
}

bool
Value::operator!=(const Value& value)
{
  return !equals(value);
}

Value::Type
Value::getType() const
{
  return (Value::Type) natus_get_type(internal);
}

const char*
Value::getTypeName() const
{
  return natus_get_type_name(internal);
}

bool
Value::isException() const
{
  return natus_is_exception(internal);
}

bool
Value::isType(Value::Type types) const
{
  return natus_is_type(internal, (natusValueType) types);
}

bool
Value::isArray() const
{
  return natus_is_array(internal);
}

bool
Value::isBoolean() const
{
  return natus_is_boolean(internal);
}

bool
Value::isFunction() const
{
  return natus_is_function(internal);
}

bool
Value::isNull() const
{
  return natus_is_null(internal);
}

bool
Value::isNumber() const
{
  return natus_is_number(internal);
}

bool
Value::isObject() const
{
  return natus_is_object(internal);
}

bool
Value::isString() const
{
  return natus_is_string(internal);
}

bool
Value::isUndefined() const
{
  return natus_is_undefined(internal);
}

Value
Value::toException()
{
  return Value(natus_to_exception(internal), false);
}

bool
Value::equals(Value val)
{
  return natus_equals(internal, val.internal);
}

bool
Value::equalsStrict(Value val)
{
  return natus_equals_strict(internal, val.internal);
}
