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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus.h>
#include <natus.hh>
using namespace natus;

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

  Value
  arrayBuilder(Value array, Value item)
  {
    const Value* const items[2] =
      { &item, NULL };
    if (!array.isArray())
      return array.newArray(items);

    array.set(array.get("length").to<size_t>(), item);
    return array;
  }

  Value
  arrayBuilder(Value array, bool item)
  {
    return arrayBuilder(array, array.newBoolean(item));
  }

  Value
  arrayBuilder(Value array, int item)
  {
    return arrayBuilder(array, array.newNumber(item));
  }

  Value
  arrayBuilder(Value array, long item)
  {
    return arrayBuilder(array, array.newNumber(item));
  }

  Value
  arrayBuilder(Value array, double item)
  {
    return arrayBuilder(array, array.newNumber(item));
  }

  Value
  arrayBuilder(Value array, const char* item)
  {
    return arrayBuilder(array, array.newString(item));
  }

  Value
  arrayBuilder(Value array, const Char* item)
  {
    return arrayBuilder(array, array.newString(item));
  }

  Value
  arrayBuilder(Value array, UTF8 item)
  {
    return arrayBuilder(array, array.newString(item));
  }

  Value
  arrayBuilder(Value array, UTF16 item)
  {
    return arrayBuilder(array, array.newString(item));
  }

  Value
  arrayBuilder(Value array, NativeFunction item)
  {
    return arrayBuilder(array, array.newFunction(item));
  }
}
