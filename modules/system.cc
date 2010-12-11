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
#include <natus/natus.h>
using namespace natus;

class EnvClass : public Class {
public:
	virtual Class::Flags getFlags () {
		return Class::FlagObject;
	}

	virtual Value del(Value& obj, string name) {
		unsetenv(name.c_str());
		return obj.newBool(true);
	}

	virtual Value get(Value& obj, string name) {
		char *value = getenv(name.c_str());
		return value ? obj.newString(value) : obj.newUndefined();
	}

	virtual Value set(Value& obj, string name, Value& value) {
		setenv(name.c_str(), value.toString().c_str(), true);
		return obj.newBool(true);
	}

	virtual Value enumerate(Value& obj) {
		extern char **environ;

		vector<Value> keys;
		for (int i=0 ; environ[i] ; i++) {
			string key = environ[i];
			if (key.find_first_of('=') == string::npos) continue;
			key = key.substr(0, key.find_first_of('='));
			keys.push_back(obj.newString(key));
		}
		return obj.newArray(keys);
	}
};

extern "C" bool natus_require(Value& base) {
	bool ok = base.set("exports.args", base.newArray());
	     ok = base.set("exports.env", base.newObject(new EnvClass())) || ok;
	return ok;
}
