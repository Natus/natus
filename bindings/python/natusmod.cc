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

#include <Python.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.h>
using namespace natus;

// From common.cc
extern Value value_from_pyobject(Value val, PyObject *obj);
extern void readyNatusTypes();

Value import(Value& ths, Value& fnc, vector<Value>& arg) {
	if (arg.size() < 1)
		return ths.newString("Missing module argument!").toException();
	if (!arg[0].isString())
		return ths.newString("Module argument must be a string!").toException();

	string modname = arg[0].toString();
	PyObject *obj = PyImport_ImportModuleLevel((char*) modname.c_str(), NULL, NULL, NULL, 0);
	if (!obj) return ths.newUndefined();

	if (modname.find_first_of('.') != string::npos)
		modname = modname.substr(0, modname.find_first_of('.'));

	Value module = value_from_pyobject(ths, obj);
	ths.set(modname, module, Value::Constant);
	return module;
}

extern "C" bool natus_require(Value& base) {
	Py_Initialize();
	readyNatusTypes();

	// Get the builtin dictionary
	PyObject *bid = PyEval_GetBuiltins();
	if (!bid) return false;

	PyObject *key = NULL;
	PyObject *val = NULL;
	Py_ssize_t pos = 0;
	while (PyDict_Next(bid, &pos, &key, &val)) {
		char *tmp = PyString_AsString(key);

		// Don't export raw import functionality since we'll export our own
		if (!strcmp(tmp, "__import__")) continue;

		Py_XINCREF(val);
		bool res = base.set(string("exports.") + tmp, value_from_pyobject(base, val), Value::Constant);
		if (!res) return false;
	}

	// Export our own import functionality
	// TODO: add whitelist capability
	return base.set("exports.import", base.newFunction(import));
}
