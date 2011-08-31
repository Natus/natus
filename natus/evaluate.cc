#include <natus-internal.hh>

Value
Value::evaluate(Value javascript, Value filename, unsigned int lineno)
{
  return natus_evaluate(internal, javascript.internal, filename.internal, lineno);
}

Value
Value::evaluate(UTF8 javascript, UTF8 filename, unsigned int lineno)
{
  Value js = newString(javascript);
  Value fn = newString(filename);
  return natus_evaluate(internal, js.internal, fn.internal, lineno);
}

Value
Value::evaluate(UTF16 javascript, UTF16 filename, unsigned int lineno)
{
  Value js = newString(javascript);
  Value fn = newString(filename);
  return natus_evaluate(internal, js.internal, fn.internal, lineno);
}
