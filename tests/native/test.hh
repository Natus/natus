#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <cassert>
#include <sys/types.h>
#include <dirent.h>

#include <iostream>
using namespace std;

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus.hh>
using namespace natus;

#include "../test.h"

int
doTest(Value& global);

static Value
alert(Value& fnc, Value& ths, Value& args)
{
  NATUS_CHECK_ARGUMENTS(fnc, "s");

  fprintf(stderr, "%s\n", args[0].to<UTF8>().c_str());
  return ths.newUndefined();
}

int
onEngine(const char *eng, int argc, const char **argv)
{
  Value global = Value::newGlobal(eng);
  if (global.isException()) {
    fprintf(stderr, "Unable to init engine! %s\n", eng);
    return 1;
  }
  printf("Engine: %s\n", global.getEngineName());

  global.set("alert", alert, Value::PropAttrProtected);
  return doTest(global);
}
