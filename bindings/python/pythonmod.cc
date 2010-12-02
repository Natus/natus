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
extern void readyNatusTypes();
extern PyObject *NatusException;
extern PyTypeObject natus_EngineType;
extern PyTypeObject natus_ValueType;

static PyMethodDef module_methods[] = {
	{NULL}
};

PyMODINIT_FUNC initnatus(void) {
	readyNatusTypes();

	PyObject* m = Py_InitModule("natus", module_methods);
	if (m == NULL) return;

	Py_INCREF(&natus_EngineType);
	PyModule_AddObject(m, "Engine", (PyObject*) &natus_EngineType);

	Py_INCREF(&natus_ValueType);
	PyModule_AddObject(m, "Value", (PyObject*) &natus_ValueType);

	Py_XINCREF(NatusException);
	PyModule_AddObject(m, "NatusException", NatusException);
}
