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
