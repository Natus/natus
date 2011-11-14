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

#ifndef NATUS_REQUIRE_H_
#define NATUS_REQUIRE_H_
#include <natus/natus.h>

#define NATUS_MODULE(modname) bool natus_module_init(natusValue *modname)
#define NATUS_CHECK_ORIGIN(ctx, uri) \
  if (!natus_require_origin_permitted(ctx, uri)) \
    return natus_throw_exception(ctx, "SecurityError", "Permission denied!");

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef enum {
  natusRequireHookStepResolve, natusRequireHookStepLoad, natusRequireHookStepProcess
} natusRequireHookStep;

typedef natusValue*
(*natusRequireHook)(natusValue *ctx, natusRequireHookStep step,
                    natusValue *name, natusValue *uri, void *misc);

typedef bool
(*natusRequireOriginMatcher)(const char *pattern, const char *subject);

typedef bool
(*natusRequireModuleInit)(natusValue *module);

bool
natus_require_init(natusValue *ctx, natusValue *config);

bool
natus_require_init_utf8(natusValue *ctx, const char *config);

natusValue *
natus_require_get_config(natusValue *ctx);

bool
natus_require_hook_add(natusValue *ctx, const char *name, natusRequireHook func,
                       void *misc, natusFreeFunction free);

bool
natus_require_hook_del(natusValue *ctx, const char *name);

bool
natus_require_origin_matcher_add(natusValue *ctx, const char *name,
                                 natusRequireOriginMatcher func,
                                 void *misc, natusFreeFunction free);

bool
natus_require_origin_matcher_del(natusValue *ctx, const char *name);

bool
natus_require_origin_permitted(natusValue *ctx, const char *uri);

natusValue *
natus_require(natusValue *ctx, natusValue *name);

natusValue *
natus_require_utf8(natusValue *ctx, const char *name);

natusValue *
natus_require_utf16(natusValue *ctx, const natusChar *name);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* NATUS_REQUIRE_H_ */

