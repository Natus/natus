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

#ifndef MODULELOADER_H_
#define MODULELOADER_H_
#include "types.h"

typedef ntValue* (*ntRequireFunction)(ntValue* module, const char* name, const char* reldir, const char** path, void* misc);
typedef bool     (*ntOriginMatcher)  (const char* pattern, const char* subject);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ntModuleLoader ntModuleLoader;

ntModuleLoader   *nt_moduleloader_init              (ntValue *ctx, const char *config);
int               nt_moduleloader_require_hook_add  (      ntModuleLoader *ml, bool post, ntRequireFunction func, void* misc, ntFreeFunction free);
void              nt_moduleloader_require_hook_del  (      ntModuleLoader *ml, int id);
int               nt_moduleloader_origin_matcher_add(      ntModuleLoader *ml, ntOriginMatcher func, void* misc, ntFreeFunction free);
void              nt_moduleloader_origin_matcher_del(      ntModuleLoader *ml, int id);
ntValue          *nt_moduleloader_require           (const ntModuleLoader *ml, const char *name, const char *reldir, const char **path);
bool              nt_moduleloader_origin_permitted  (const ntModuleLoader *ml, const char *uri);
void              nt_moduleloader_free              (      ntModuleLoader *ml);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* MODULELOADER_H_ */

