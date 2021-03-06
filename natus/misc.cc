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

#include <natus-internal.hh>

namespace natus
{
  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, va_list ap)
  {
    return natus_throw_exception_varg(ctx.borrowCValue(), base, type, format, ap);
  }

  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    Value exc = throwException (ctx, base, type, format, ap);
    va_end(ap);
    return exc;
  }

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, va_list ap)
  {
    return natus_throw_exception_code_varg(ctx.borrowCValue(), base, type, code, format, ap);
  }

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    Value exc = throwException (ctx, base, type, code, format, ap);
    va_end(ap);
    return exc;
  }

  Value
  throwException(Value ctx, int errorno)
  {
    return natus_throw_exception_errno(ctx.borrowCValue(), errorno);
  }

  Value
  ensureArguments(Value args, const char* fmt)
  {
    return natus_ensure_arguments(args.borrowCValue(), fmt);
  }

  Value
  convertArguments(Value args, const char *fmt, va_list ap)
  {
    return natus_convert_arguments_varg(args.borrowCValue(), fmt, ap);
  }

  Value
  convertArguments(Value args, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Value ret = convertArguments (args, fmt, ap);
    va_end(ap);
    return ret;
  }
}

