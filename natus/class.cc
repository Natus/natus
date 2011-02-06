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
#include "natus.h"
namespace natus {

Class::Flags Class::getFlags() {
	return Class::FlagNone;
}

Value Class::del(Value& obj, string name) {
	return obj.newUndefined().toException();
}

Value Class::del(Value& obj, long idx) {
	return obj.newUndefined().toException();
}

Value Class::get(Value& obj, string name) {
	return obj.newUndefined().toException();
}

Value Class::get(Value& obj, long idx) {
	return obj.newUndefined().toException();
}

Value Class::set(Value& obj, string name, Value& value) {
	return obj.newUndefined().toException();
}

Value Class::set(Value& obj, long idx, Value& value) {
	return obj.newUndefined().toException();
}

Value Class::enumerate(Value& obj) {
	return obj.newUndefined().toException();
}

Value Class::call(Value& obj, vector<Value> args) {
	return obj.newUndefined().toException();
}

Value Class::callNew(Value& obj, vector<Value> args) {
	return obj.newUndefined().toException();
}

Class::~Class() {}

}
