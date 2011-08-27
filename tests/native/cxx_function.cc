#include "test.hh"
#include <string.h>

static Value
firstarg_function(Value& fnc, Value& ths, Value& arg)
{
  assert(fnc.borrowCValue ());
  assert(ths.borrowCValue ());
  assert(arg.borrowCValue ());

  assert(ths.isGlobal () || ths.isUndefined ());
  assert(fnc.isFunction ());
  assert(arg.isArray ());
  assert(arg.get ("length").to<int> () > 0);
  return arg.get(0);
}

static Value
exception_function(Value& fnc, Value& ths, Value& arg)
{
  return firstarg_function(fnc, ths, arg).toException();
}

int
doTest(Value& global)
{
  Value glbl = Value::newGlobal(global);
  Value rslt;
  Value func = global.newFunction(firstarg_function);
  assert(func.isFunction ());
  assert(!global.set ("x", func).isException ());
  assert(!glbl.set ("x", global.get ("x")).isException ());
  assert(global.get ("x").isFunction ());
  assert(glbl.get ("x").isFunction ());

  // Call from C++
  rslt = global.call("x", arrayBuilder(global, 123));
  assert(!rslt.isException ());
  assert(123 == rslt.to<int> ());
  rslt = glbl.call("x", arrayBuilder(glbl, 123));
  assert(!rslt.isException ());
  assert(123 == rslt.to<int> ());

  // New from C++
  rslt = global.callNew("x", arrayBuilder(global, global.newObject()));
  assert(!rslt.isException ());
  assert(rslt.isObject ());
  rslt = glbl.callNew("x", arrayBuilder(glbl, glbl.newObject()));
  assert(!rslt.isException ());
  assert(rslt.isObject ());

  // Call from JS
  rslt = global.evaluate("x(123);");
  assert(!rslt.isException ());
  assert(123 == rslt.to<int> ());
  rslt = glbl.evaluate("x(123);");
  assert(!rslt.isException ());
  assert(123 == rslt.to<int> ());

  // New from JS
  rslt = global.evaluate("new x({});");
  assert(!rslt.isException ());
  assert(rslt.isObject ());
  rslt = glbl.evaluate("new x({});");
  assert(!rslt.isException ());
  assert(rslt.isObject ());

  assert(!global.del ("x").isException ());
  assert(!glbl.del ("x").isException ());
  assert(global.get ("x").isUndefined ());
  assert(glbl.get ("x").isUndefined ());
  assert(!global.set ("x", global.newFunction (exception_function)).isException ());
  assert(!glbl.set ("x", global.get ("x")).isException ());

  // Exception Call from C++
  rslt = global.call("x", arrayBuilder(global, 123));
  assert(rslt.isException ());
  rslt = glbl.call("x", arrayBuilder(glbl, 123));
  assert(rslt.isException ());

  // Exception New from C++
  rslt = global.callNew("x", arrayBuilder(global, global.newObject()));
  assert(rslt.isException ());
  rslt = glbl.callNew("x", arrayBuilder(glbl, glbl.newObject()));
  assert(rslt.isException ());

  // Exception Call from JS
  rslt = global.evaluate("x(123);");
  assert(rslt.isException ());
  rslt = glbl.evaluate("x(123);");
  assert(rslt.isException ());

  // Exception New from JS
  rslt = global.evaluate("new x({});");
  assert(rslt.isException ());
  rslt = glbl.evaluate("new x({});");
  assert(rslt.isException ());

  // TODO: OOM

  // Cleanup
  assert(!global.del ("x").isException ());
  assert(!glbl.del ("x").isException ());
  assert(global.get ("x").isUndefined ());
  assert(glbl.get ("x").isUndefined ());
  return 0;
}
