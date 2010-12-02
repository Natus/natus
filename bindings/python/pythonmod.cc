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
