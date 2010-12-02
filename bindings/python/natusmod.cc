/*
 * Copyright 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * This file is part of Natus.
 *
 * Natus is free software: you can redistribute it and/or modify it under the
 * terms of either 1. the GNU General Public License version 2.1, as published
 * by the Free Software Foundation or 2. the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3.0 of the
 * License, or (at your option) any later version.
 *
 * Natus is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with Natus. If not, see
 * http://www.gnu.org/licenses/.
 */

#include "common.cc"

Value import(Value& ths, Value& fnc, vector<Value>& arg, void* msc) {
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
