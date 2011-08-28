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

#ifndef MISCXX_HPP_
#define MISCXX_HPP_
#include "types.hh"
#include "value.hh"
#include <cstdarg>

#undef NATUS_CHECK_ARGUMENTS
#define NATUS_CHECK_ARGUMENTS(arg, fmt) { \
  Value _exc = ensureArguments(arg, fmt); \
  if (_exc.isException()) return _exc; }

namespace natus
{
  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, va_list ap);

  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, ...);

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, va_list ap);

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, ...);

  Value
  throwException(Value ctx, int errorno);

  Value
  ensureArguments(Value args, const char* fmt);

  Value
  convertArguments(Value args, const char *fmt, va_list ap);

  Value
  convertArguments(Value args, const char *fmt, ...);

  Value
  arrayBuilder(Value array, Value item);

  Value
  arrayBuilder(Value array, bool item);

  Value
  arrayBuilder(Value array, int item);

  Value
  arrayBuilder(Value array, long item);

  Value
  arrayBuilder(Value array, double item);

  Value
  arrayBuilder(Value array, const char* item);

  Value
  arrayBuilder(Value array, const Char* item);

  Value
  arrayBuilder(Value array, UTF8 item);

  Value
  arrayBuilder(Value array, UTF16 item);

  Value
  arrayBuilder(Value array, NativeFunction item);
}
#endif /* MISCXX_HPP_ */

