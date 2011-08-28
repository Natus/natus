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

#ifndef MISC_H_
#define MISC_H_
#include "types.h"
#include <stdarg.h>

#define NATUS_CHECK_ARGUMENTS(arg, fmt) \
{ \
  natusValue *_exc = natus_ensure_arguments(arg, fmt); \
  if (natus_is_exception(_exc)) return _exc; \
  natus_decref(_exc); \
}

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
natusValue *
natus_throw_exception(const natusValue *ctx, const char *base,
                      const char *type, const char *format, ...);

natusValue *
natus_throw_exception_varg(const natusValue *ctx, const char *base,
                           const char *type, const char *format, va_list ap);

natusValue *
natus_throw_exception_code(const natusValue *ctx, const char *base,
                           const char *type, int code, const char *format,
                           ...);

natusValue *
natus_throw_exception_code_varg(const natusValue *ctx, const char *base,
                                const char *type, int code, const char *format,
                                va_list ap);

natusValue *
natus_throw_exception_errno(const natusValue *ctx, int errorno);

natusValue *
natus_ensure_arguments(natusValue *arg, const char *fmt);

natusValue *
natus_convert_arguments(natusValue *arg, const char *fmt, ...);

natusValue *
natus_convert_arguments_varg(natusValue *arg, const char *fmt, va_list ap);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* MISC_H_ */

