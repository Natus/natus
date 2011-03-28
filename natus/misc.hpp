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
#include "types.hpp"
#include "value.hpp"

namespace natus {

Value throwException(Value ctx, const char* type, const char* message);
Value throwException(Value ctx, const char* type, const char* message, long code);
Value throwException(Value ctx, int errorno);
Value checkArguments(Value ctx, Value** args, const char* fmt);

Value fromJSON(Value json);
Value fromJSON(Value ctx, UTF8 json);
Value fromJSON(Value ctx, UTF16 json);
Value toJSON(Value val);

}
#endif /* MISCXX_HPP_ */
