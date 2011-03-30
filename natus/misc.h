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

#define NT_CHECK_ARGUMENTS(arg, fmt) \
	ntValue *_exc = nt_check_arguments(arg, fmt); \
	if (nt_value_is_exception(_exc)) return _exc; \
	nt_value_decref(_exc);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char    *nt_vsprintf(const char *format, va_list ap);
char    *nt_sprintf(const char *format, ...);

ntValue *nt_throw_exception          (const ntValue *ctx, const char *type, const char *format, ...);
ntValue *nt_throw_exception_varg     (const ntValue *ctx, const char *type, const char *format, va_list ap);
ntValue *nt_throw_exception_code     (const ntValue *ctx, const char *type, int code, const char *format, ...);
ntValue *nt_throw_exception_code_varg(const ntValue *ctx, const char *type, int code, const char *format, va_list ap);
ntValue *nt_throw_exception_errno    (const ntValue *ctx, int errorno);
ntValue *nt_check_arguments          (const ntValue *arg, const char *fmt);

ntValue *nt_from_json                (const ntValue *json);
ntValue *nt_from_json_utf8           (const ntValue *ctx, const    char *json, size_t len);
ntValue *nt_from_json_utf16          (const ntValue *ctx, const  ntChar *json, size_t len);
ntValue *nt_to_json                  (const ntValue *val);
char    *nt_to_json_utf8             (const ntValue *val, size_t *len);
ntChar  *nt_to_json_utf16            (const ntValue *val, size_t *len);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* MISC_H_ */

