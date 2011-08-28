#include "test.hh"

static Value
function(Value& fnc, Value& ths, Value& arg)
{
  return fnc.newBoolean(true);
}

int
doTest(Value& global)
{
  Value array = global.newArray();
  Value boolean = global.newBoolean(true);
  Value func = global.newFunction(function);
  Value null = global.newNull();
  Value number = global.newNumber(17);
  Value object = global.newObject();
  Value string = global.newString("hi");
  Value undefined = global.newUndefined();
  Value nullv = NULL;

  assert(array.isArray());
  assert(!array.isBoolean());
  assert(!array.isFunction());
  assert(!array.isNull());
  assert(!array.isNumber());
  assert(!array.isObject());
  assert(!array.isString());
  assert(!array.isUndefined());
  assert(!array.isException());
  assert(array.toException().isException());

  assert(!boolean.isArray());
  assert(boolean.isBoolean());
  assert(!boolean.isFunction());
  assert(!boolean.isNull());
  assert(!boolean.isNumber());
  assert(!boolean.isObject());
  assert(!boolean.isString());
  assert(!boolean.isUndefined());
  assert(!boolean.isException());
  assert(boolean.toException().isException());

  assert(!func.isArray());
  assert(!func.isBoolean());
  assert(func.isFunction());
  assert(!func.isNull());
  assert(!func.isNumber());
  assert(!func.isObject());
  assert(!func.isString());
  assert(!func.isUndefined());
  assert(!func.isException());
  assert(func.toException().isException());

  assert(!null.isArray());
  assert(!null.isBoolean());
  assert(!null.isFunction());
  assert(null.isNull());
  assert(!null.isNumber());
  assert(!null.isObject());
  assert(!null.isString());
  assert(!null.isUndefined());
  assert(!null.isException());
  assert(null.toException().isException());

  assert(!number.isArray());
  assert(!number.isBoolean());
  assert(!number.isFunction());
  assert(!number.isNull());
  assert(number.isNumber());
  assert(!number.isObject());
  assert(!number.isString());
  assert(!number.isUndefined());
  assert(!number.isException());
  assert(number.toException().isException());

  assert(!object.isArray());
  assert(!object.isBoolean());
  assert(!object.isFunction());
  assert(!object.isNull());
  assert(!object.isNumber());
  assert(object.isObject());
  assert(!object.isString());
  assert(!object.isUndefined());
  assert(!object.isException());
  assert(object.toException().isException());

  assert(!string.isArray());
  assert(!string.isBoolean());
  assert(!string.isFunction());
  assert(!string.isNull());
  assert(!string.isNumber());
  assert(!string.isObject());
  assert(string.isString());
  assert(!string.isUndefined());
  assert(!string.isException());
  assert(string.toException().isException());

  assert(!undefined.isArray());
  assert(!undefined.isBoolean());
  assert(!undefined.isFunction());
  assert(!undefined.isNull());
  assert(!undefined.isNumber());
  assert(!undefined.isObject());
  assert(!undefined.isString());
  assert(undefined.isUndefined());
  assert(!undefined.isException());
  assert(undefined.toException().isException());

  assert(!nullv.isArray());
  assert(!nullv.isBoolean());
  assert(!nullv.isFunction());
  assert(!nullv.isNull());
  assert(!nullv.isNumber());
  assert(!nullv.isObject());
  assert(!nullv.isString());
  assert(nullv.isUndefined());
  assert(nullv.isException());

  return 0;
}
