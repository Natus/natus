#include <natus-internal.hh>

bool
Value::operator==(const Value& value) const
{
  return equals(value);
}

bool
Value::operator==(bool value) const
{
  return equals(value);
}

bool
Value::operator==(int value) const
{
  return equals(value);
}

bool
Value::operator==(long value) const
{
  return equals(value);
}

bool
Value::operator==(double value) const
{
  return equals(value);
}

bool
Value::operator==(const char* value) const
{
  return equals(value);
}

bool
Value::operator==(const Char* value) const
{
  return equals(value);
}

bool
Value::operator==(UTF8 value) const
{
  return equals(value);
}

bool
Value::operator==(UTF16 value) const
{
  return equals(value);
}

bool
Value::operator!=(const Value& value) const
{
  return !equals(value);
}

bool
Value::operator!=(bool value) const
{
  return !equals(value);
}

bool
Value::operator!=(int value) const
{
  return !equals(value);
}

bool
Value::operator!=(long value) const
{
  return !equals(value);
}

bool
Value::operator!=(double value) const
{
  return !equals(value);
}

bool
Value::operator!=(const char* value) const
{
  return !equals(value);
}

bool
Value::operator!=(const Char* value) const
{
  return !equals(value);
}

bool
Value::operator!=(UTF8 value) const
{
  return !equals(value);
}

bool
Value::operator!=(UTF16 value) const
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
Value::equals(Value val) const
{
  return natus_equals(internal, val.internal);
}

bool
Value::equals(bool value) const
{
  return equals(newBoolean(value));
}

bool
Value::equals(int value) const
{
  return equals(newNumber(value));
}

bool
Value::equals(long value) const
{
  return equals(newNumber(value));
}

bool
Value::equals(double value) const
{
  return equals(newNumber(value));
}

bool
Value::equals(const char* value) const
{
  return equals(newString(value));
}

bool
Value::equals(const Char* value) const
{
  return equals(newString(value));
}

bool
Value::equals(UTF8 value) const
{
  return equals(newString(value));
}

bool
Value::equals(UTF16 value) const
{
  return equals(newString(value));
}

bool
Value::equalsStrict(Value val) const
{
  return natus_equals_strict(internal, val.internal);
}

bool
Value::equalsStrict(bool value) const
{
  return equalsStrict(newBoolean(value));
}

bool
Value::equalsStrict(int value) const
{
  return equalsStrict(newNumber(value));
}

bool
Value::equalsStrict(long value) const
{
  return equalsStrict(newNumber(value));
}

bool
Value::equalsStrict(double value) const
{
  return equalsStrict(newNumber(value));
}

bool
Value::equalsStrict(const char* value) const
{
  return equalsStrict(newString(value));
}

bool
Value::equalsStrict(const Char* value) const
{
  return equalsStrict(newString(value));
}

bool
Value::equalsStrict(UTF8 value) const
{
  return equalsStrict(newString(value));
}

bool
Value::equalsStrict(UTF16 value) const
{
  return equalsStrict(newString(value));
}
