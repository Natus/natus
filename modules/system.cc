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
#include "iocommon.hpp"

#ifdef __linux__
static inline const char** __getenviron() {
	extern char **environ;
	return (const char**) environ;
}
#endif
#ifdef __APPLE__
#include <crt_externs.h>
static inline const char** __getenviron() {
	return (const char**) (*_NSGetEnviron());
}
#endif

class EnvClass : public Class {
public:
	virtual Class::Flags getFlags () {
		return Class::FlagObject;
	}

	virtual Value del(Value& obj, Value& idx) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		unsetenv(idx.to<UTF8>().c_str());
		return obj.newUndefined();
	}

	virtual Value get(Value& obj, Value& idx) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		char *value = getenv(idx.to<UTF8>().c_str());
		return value ? obj.newString(value) : obj.newUndefined();
	}

	virtual Value set(Value& obj, Value& idx, Value& value) {
		if (!idx.isString()) return throwException(obj, "PropertyError", "Not found!");

		setenv(idx.to<UTF8>().c_str(), value.to<UTF8>().c_str(), true);
		return obj.newBoolean(true);
	}

	virtual Value enumerate(Value& obj) {
		const char **environ = __getenviron();

		Value properties = obj.newArray();
		for (int i=0 ; environ[i] ; i++) {
			UTF8 key = environ[i];
			if (key.find_first_of('=') == UTF8::npos) continue;
			arrayBuilder(properties, key.substr(0, key.find_first_of('=')));
		}
		return properties;
	}
};

extern "C" bool NATUS_MODULE_INIT(ntValue* module) {
	Value base(module, false);

	Value ostdin  = base.newObject();
	Value ostdout = base.newObject();
	Value ostderr = base.newObject();
	stream_from_fd(ostdin,  STDIN_FILENO);
	stream_from_fd(ostdout, STDOUT_FILENO);
	stream_from_fd(ostderr, STDERR_FILENO);

	bool ok = !base.setRecursive("exports.args", base.newArray()).isException();
	     ok = !base.setRecursive("exports.env", base.newObject(new EnvClass())).isException() || ok;
	     ok = !base.setRecursive("exports.stdin",  ostdin).isException() || ok;
	     ok = !base.setRecursive("exports.stdout", ostdout).isException() || ok;
	     ok = !base.setRecursive("exports.stderr", ostderr).isException() || ok;
	return ok;
}
