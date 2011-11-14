/*
 * Copyright (c) 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#ifndef NATUS_REQUIRE_HH_
#define NATUS_REQUIRE_HH_
#include <natus.hh>

#undef NATUS_MODULE
#define NATUS_MODULE(modname) \
  bool natus_module_init_cxx(Value modname); \
  extern "C" bool natus_module_init(natusValue *module) { \
    return natus_module_init_cxx(Value(module, false)); \
  } \
  bool natus_module_init_cxx(Value modname)

#undef NATUS_CHECK_ORIGIN
#define NATUS_CHECK_ORIGIN(ctx, uri) \
  if (!natus::require::originPermitted(ctx, uri)) \
    return throwException(ctx, "SecurityError", "Permission denied!")

namespace natus
{
  namespace require
  {
    typedef enum {
      HookStepResolve, HookStepLoad, HookStepProcess
    } HookStep;

    typedef Value
    (*Hook)(Value ctx, HookStep step, Value name, Value uri, void* misc);

    typedef bool
    (*OriginMatcher)(const char* pattern, const char* subject);

    typedef bool
    (*ModuleInit)(natusValue* module);

    bool
    addHook(Value ctx, const char* name, Hook func,
            void* misc=NULL, FreeFunction free=NULL);

    bool
    addOriginMatcher(Value ctx, const char* name, OriginMatcher func,
                     void* misc=NULL, FreeFunction free=NULL);

    bool
    delHook(Value ctx, const char* name);

    bool
    delOriginMatcher(Value ctx, const char* name);

    bool
    init(Value ctx, const char* config);

    bool
    init(Value ctx, UTF8 config);

    bool
    init(Value ctx, Value config);

    Value
    getConfig(Value ctx);

    bool
    originPermitted(Value ctx, const char* name);

    Value
    require(Value ctx, UTF8 name);

    Value
    require(Value ctx, UTF16 name);

    Value
    require(Value ctx, Value name);
  }
}
#endif /* NATUS_REQUIRE_HH_ */

