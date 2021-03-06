#include "test.hh"

int
doTest(Value& global)
{
  Value array = global.newArray().push(123).push(456);
  assert(array.isArray());
  assert(array.get("length").to<int>() == 2);
  assert(!global.set("x", array).isException());

  array = global.get("x");
  assert(array.isArray());
  assert(2 == array.get("length").to<int>());
  assert(3 == array.call("push", global.newArray().push("foo")).to<int>());

  assert(123 == array.get(0).to<int>());
  assert(456 == array.get(1).to<int>());
  assert(array.get(2).to<UTF8>() == "foo");

  Value v = array.call("pop");
  assert(123 == array.get(0).to<int>());
  assert(456 == array.get(1).to<int>());
  assert(v.isString());
  assert(v.to<UTF8>() == "foo");

  assert(!array.set(0, v.newNumber(789)).isException());
  assert(789 == array.get(0).to<int>());
  assert(456 == array.get(1).to<int>());

  assert(!global.del("x").isException());
  assert(global.get("x").isUndefined());
  return 0;
}
