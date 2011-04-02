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

#include "misc.h"
#include "value.hpp"
using namespace natus;

namespace natus {

char* vsprintf(const char* format, va_list ap) {
	return nt_vsprintf(format, ap);
}

char* sprintf(const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	char *tmp = nt_vsprintf(format, ap);
	va_end(ap);
	return tmp;
}

Value throwException(Value ctx, const char* type, const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	Value exc = throwException(ctx, type, format, ap);
	va_end(ap);
	return exc;
}

Value throwException(Value ctx, const char* type, const char* format, va_list ap) {
	return nt_throw_exception_varg(ctx.borrowCValue(), type, format, ap);
}

Value throwException(Value ctx, const char* type, int code, const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	Value exc = throwException(ctx, type, code, format, ap);
	va_end(ap);
	return exc;
}

Value throwException(Value ctx, const char* type, int code, const char* format, va_list ap) {
	return nt_throw_exception_code_varg(ctx.borrowCValue(), type, code, format, ap);
}

Value throwException(Value ctx, int errorno) {
	return nt_throw_exception_errno(ctx.borrowCValue(), errorno);
}

Value ensureArguments(Value args, const char* fmt) {
	return nt_ensure_arguments(args.borrowCValue(), fmt);
}

Value fromJSON(Value json) {
	return nt_from_json(json.borrowCValue());
}

Value fromJSON(Value ctx, UTF8 json) {
	return fromJSON(ctx.newString(json));
}

Value fromJSON(Value ctx, UTF16 json) {
	return fromJSON(ctx.newString(json));
}

Value toJSON(Value val) {
	return nt_to_json(val.borrowCValue());
}

}
