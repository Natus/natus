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

#include <stdlib.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.hpp>
using namespace natus;

class EnvClass : public Class {
public:
	virtual Class::Flags getFlags () {
		return Class::FlagObject;
	}

	virtual Value del(Value& obj, Value& idx) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		unsetenv(idx.toStringUTF8().c_str());
		return obj.newBool(true);
	}

	virtual Value get(Value& obj, Value& idx) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		char *value = getenv(idx.toStringUTF8().c_str());
		return value ? obj.newString(value) : obj.newUndefined();
	}

	virtual Value set(Value& obj, Value& idx, Value& value) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		setenv(idx.toStringUTF8().c_str(), value.toStringUTF8().c_str(), true);
		return obj.newBool(true);
	}

	virtual Value enumerate(Value& obj) {
		extern char **environ;

		Value properties = obj.newArray();
		for (int i=0 ; environ[i] ; i++) {
			UTF8 key = environ[i];
			if (key.find_first_of('=') == UTF8::npos) continue;
			properties.newArrayBuilder(key.substr(0, key.find_first_of('=')));
		}
		return properties;
	}
};

extern "C" bool NATUS_MODULE_INIT(ntValue* module) {
	Value base(module, false);
	bool ok = !base.set("exports.args", base.newArray()).isException();
	     ok = !base.set("exports.env", base.newObject(new EnvClass())).isException() || ok;
	return ok;
}
