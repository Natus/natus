#include <natus-internal.hh>

Value
Value::operator[](long index)
{
  return get(index);
}

Value
Value::operator[](Value& name)
{
  return get(name);
}

Value
Value::operator[](UTF8 name)
{
  return get(name);
}

Value
Value::operator[](UTF16 name)
{
  return get(name);
}

Value
Value::del(Value idx)
{
  return natus_del(internal, idx.internal);
}

Value
Value::del(UTF8 idx)
{
  Value n = newString(idx);
  return natus_del(internal, n.internal);
}

Value
Value::del(UTF16 idx)
{
  Value n = newString(idx);
  return natus_del(internal, n.internal);
}

Value
Value::del(size_t idx)
{
  return natus_del_index(internal, idx);
}

Value
Value::delRecursive(UTF8 path)
{
  return natus_del_recursive_utf8(internal, path.c_str());
}

Value
Value::get(Value idx) const
{
  return natus_get(internal, idx.internal);
}

Value
Value::get(UTF8 idx) const
{
  Value n = newString(idx);
  return natus_get(internal, n.internal);
}

Value
Value::get(UTF16 idx) const
{
  Value n = newString(idx);
  return natus_get(internal, n.internal);
}

Value
Value::get(size_t idx) const
{
  return natus_get_index(internal, idx);
}

Value
Value::getRecursive(UTF8 path)
{
  return natus_get_recursive_utf8(internal, path.c_str());
}

Value
Value::set(Value idx, Value value, Value::PropAttr attrs)
{
  return natus_set(internal, idx.internal, value.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, bool value, Value::PropAttr attrs)
{
  Value v = newBoolean(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, int value, Value::PropAttr attrs)
{
  Value v = newNumber(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, long value, Value::PropAttr attrs)
{
  Value v = newNumber(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, double value, Value::PropAttr attrs)
{
  Value v = newNumber(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, const char* value, Value::PropAttr attrs)
{
  Value v = newString(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, const Char* value, Value::PropAttr attrs)
{
  Value v = newString(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, UTF8 value, Value::PropAttr attrs)
{
  Value v = newString(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, UTF16 value, Value::PropAttr attrs)
{
  Value v = newString(value);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(Value idx, NativeFunction value, Value::PropAttr attrs)
{
  Value v = newFunction(value, idx.isString() ? idx.to<UTF8>().c_str() : NULL);
  return natus_set(internal, idx.internal, v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, Value value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  return natus_set(internal, n.borrowCValue(), value.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, bool value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newBoolean(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, int value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, long value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, double value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, const char* value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, const Char* value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, UTF8 value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, UTF16 value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF8 idx, NativeFunction value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newFunction(value, idx.c_str());
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, Value value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  return natus_set(internal, n.borrowCValue(), value.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, bool value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newBoolean(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, int value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, long value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, double value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newNumber(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, const char* value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, const Char* value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, UTF8 value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, UTF16 value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newString(value);
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(UTF16 idx, NativeFunction value, Value::PropAttr attrs)
{
  Value n = newString(idx);
  Value v = newFunction(value, n.to<UTF8>().c_str());
  return natus_set(internal, n.borrowCValue(), v.internal, (natusPropAttr) attrs);
}

Value
Value::set(size_t idx, Value value)
{
  return natus_set_index(internal, idx, value.internal);
}

Value
Value::set(size_t idx, bool value)
{
  Value v = newBoolean(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, int value)
{
  Value v = newNumber(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, long value)
{
  Value v = newNumber(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, double value)
{
  Value v = newNumber(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, const char* value)
{
  Value v = newString(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, const Char* value)
{
  Value v = newString(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, UTF8 value)
{
  Value v = newString(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, UTF16 value)
{
  Value v = newString(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::set(size_t idx, NativeFunction value)
{
  Value v = newFunction(value);
  return natus_set_index(internal, idx, v.internal);
}

Value
Value::setRecursive(UTF8 path, Value val, Value::PropAttr attrs, bool mkpath)
{
  return natus_set_recursive_utf8(internal, path.c_str(), val.internal, (natusPropAttr) attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, bool val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newBoolean(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, int val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, long val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, double val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newNumber(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, const char* val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newString(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, const Char* val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newString(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, UTF8 val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newString(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, UTF16 val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newString(val), attrs, mkpath);
}

Value
Value::setRecursive(UTF8 path, NativeFunction val, Value::PropAttr attrs, bool mkpath)
{
  return setRecursive(path, newFunction(val), attrs, mkpath);
}

Value
Value::enumerate() const
{
  return natus_enumerate(internal);
}
