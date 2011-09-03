#include "test.hh"
#include <natus-require.hh>

static bool freecalled = false;
void
hook_free(void* misc)
{
  assert(misc == (void*) 0x1234);
  freecalled = true;
}

Value
hook(Value ctx, require::HookStep step, Value name, Value uri, void* misc)
{
  if (name != "__internal__")
    return NULL;

  if (step == require::HookStepResolve)
    return name;

  if (step == require::HookStepLoad) {
    Value mod = ctx.newObject();
    assert(!mod.set("misc", (long) misc).isException());
    return mod;
  }

  return NULL;
}

int
doTest(Value& global)
{
  Value config = global.newObject();
  Value path = global.newArray().push("./");
  config.setRecursive("natus.require.path", path, Value::PropAttrNone, true);

  // Initialize the require system
  assert(require::expose(global, config));
  assert(require::addHook(global, "__internal__", hook, (void*) 0x1234, hook_free));

  // Load the internal module
  Value modulea = require::require(global, "__internal__");
  assert(!modulea.isException());
  assert(modulea.isObject());
  assert(modulea.get("misc").to<int>() == 0x1234);

  // Load the internal module again
  Value moduleb = require::require(global, "__internal__");
  assert(!moduleb.isException());
  assert(moduleb.isObject());
  assert(moduleb.get("misc").to<int>() == 0x1234);

  // Check to make sure that the underlying values refer to the same object
  assert(!modulea.set("test", 17L).isException());
  assert(moduleb.get("test").to<int>() == 17);

  // Load a script file via standard spec
  Value scriptmod = require::require(global, "scriptmod");
  assert(!scriptmod.isException());
  assert(scriptmod.isObject());
  assert(scriptmod.get("number").to<int>() == 115);
  assert(scriptmod.get("string").to<UTF8>() == "hello world");

  // Load a script file via relative spec
  scriptmod = require::require(global, "./scriptmod");
  assert(!scriptmod.isException());
  assert(scriptmod.isObject());
  assert(scriptmod.get("number").to<int>() == 115);
  assert(scriptmod.get("string").to<UTF8>() == "hello world");

  // Cleanup
  return 0;
}
