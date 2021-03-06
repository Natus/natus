#include "test.hh"

int
doTest(Value& global)
{
  global.set("x", global.newNull());
  assert(global.get("x").isNull());

  // Cleanup
  assert(!global.del("x").isException());
  assert(global.get("x").isUndefined());
  return 0;
}
