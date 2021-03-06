#include "test.hh"

static Value
doNothing(Value& fnc, Value& ths, Value& arg)
{
  return fnc.newBoolean(true);
}

static void
testInternal(Value& global, const char* js, Value::Type type)
{
  // Test private on an internal function
  global.evaluate(js);
  Value x = global.get("x");
  assert(x.isType(type));
  assert(!x.setPrivateName("test", (void *) 0x1234));
  assert(NULL == x.getPrivateName<void*>("test"));
  assert(!global.del("x").isException());
}

static void
testExternal(Value& global, Value val, Value::Type type, bool works = false)
{
  assert(!global.set("x", val).isException());
  Value x = global.get("x");
  assert(x.isType(type));
  if (works) {
    assert(x.setPrivateName("test", (void *) 0x1234));
    assert(0x1234 == x.getPrivateName<long>("test"));
  } else {
    assert(!x.setPrivateName("test", (void *) 0x1234));
    assert(NULL == x.getPrivateName<void*>("test"));
  }
  assert(!global.del("x").isException());
}

int
doTest(Value& global)
{
  // Ensure we can store private on the global
  assert(global.setPrivateName("test", (void *) 0x1234));
  assert(0x1234 == global.getPrivateName<long>("test"));

  // Test private on an externally defined types
  testExternal(global, global.newArray(), Value::TypeArray);
  testExternal(global, global.newBoolean(true), Value::TypeBool);
  testExternal(global, global.newFunction(doNothing), Value::TypeFunction, true);
  testExternal(global, global.newNull(), Value::TypeNull);
  testExternal(global, global.newNumber(1234), Value::TypeNumber);
  testExternal(global, global.newObject(), Value::TypeObject, true);
  testExternal(global, global.newString("hi"), Value::TypeString);
  testExternal(global, global.newUndefined(), Value::TypeUndefined);

  // Test private on an internally defined types
  testInternal(global, "x = [];", Value::TypeArray);
  testInternal(global, "x = true;", Value::TypeBool);
  testInternal(global, "x = function() {};", Value::TypeFunction);
  testInternal(global, "x = null;", Value::TypeNull);
  testInternal(global, "x = 1234;", Value::TypeNumber);
  testInternal(global, "x = {};", Value::TypeObject);
  testInternal(global, "x = 'hello';", Value::TypeString);
  testInternal(global, "", Value::TypeUndefined);

  // Cleanup
  assert(global.get("x").isUndefined());
  return 0;
}
