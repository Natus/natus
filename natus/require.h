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

#ifndef MODULES_H_
#define MODULES_H_
#include "types.h"
#include "value.h"
#include "misc.h"

#define NT_MODULE(modname) bool natus_module_init(ntValue *modname)
#define NT_CHECK_ORIGIN(ctx, uri) \
  if (!nt_require_origin_permitted(ctx, uri)) \
    return nt_throw_exception(ctx, "SecurityError", "Permission denied!");

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef enum {
  ntRequireHookStepResolve, ntRequireHookStepLoad, ntRequireHookStepProcess
} ntRequireHookStep;

typedef ntValue*
(*ntRequireHook)(ntValue *ctx, ntRequireHookStep step, char *name, void *misc);

typedef bool
(*ntRequireOriginMatcher)(const char *pattern, const char *subject);

typedef bool
(*ntRequireModuleInit)(ntValue *module);

bool
nt_require_init(ntValue *ctx, const char *config);

bool
nt_require_init_value(ntValue *ctx, ntValue *config);

ntValue *
nt_require_get_config(ntValue *ctx);

bool
nt_require_hook_add(ntValue *ctx, const char *name, ntRequireHook func,
                    void *misc, ntFreeFunction free);

bool
nt_require_hook_del(ntValue *ctx, const char *name);

bool
nt_require_origin_matcher_add(ntValue *ctx, const char *name,
                              ntRequireOriginMatcher func,
                              void *misc, ntFreeFunction free);

bool
nt_require_origin_matcher_del(ntValue *ctx, const char *name);

bool
nt_require_origin_permitted(ntValue *ctx, const char *uri);

ntValue *
nt_require(ntValue *ctx, const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* MODULES_H_ */

