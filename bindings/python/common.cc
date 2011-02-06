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

static PyObject* pyobject_from_value(Value val);
Value value_from_pyobject(Value val, PyObject *obj);

PyObject *NatusException = NULL;
PyTypeObject natus_EngineType = {
	PyObject_HEAD_INIT(NULL)
	0, "natus.Engine"
};
PyTypeObject natus_ValueType = {
	PyObject_HEAD_INIT(NULL)
	0, "natus.Value"
};

typedef struct {
    PyObject_HEAD
    Engine engine;
} natus_Engine;

typedef struct {
    PyObject_HEAD
    Value value;
} natus_Value;

class PythonObjectClass : public Class {
public:
	virtual Class::Flags getFlags () {
		return (Class::Flags) (Class::FlagArray | Class::FlagObject);
	}

	virtual Value del(Value& obj, long idx) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject* key = PyLong_FromLong(idx);
		if (!key) return obj.newBool(false);

		PyObject_DelItem(pyobj, key);
		Py_XDECREF(key);

		return obj.newBool(true);
	}

	virtual Value del(Value& obj, string name) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject_DelAttrString(pyobj, name.c_str());
		return obj.newBool(true);
	}

	virtual Value get(Value& obj, long idx) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject* key = PyLong_FromLong(idx);
		if (!key) return obj.newUndefined();

		PyObject *rval = PyObject_GetItem(pyobj, key);
		Py_XDECREF(key);
		if (PyErr_Occurred()) {
			PyErr_Clear();
			return obj.newUndefined();
		}
		return value_from_pyobject(obj, rval);
	}

	virtual Value get(Value& obj, string name) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		// First check to see if the attribute exists
		const char* attr = name.c_str();
		PyObject *rval = PyObject_GetAttrString(pyobj, attr);
		if (PyErr_Occurred()) {
			PyErr_Clear();
			// If the attribute doesn't exist, lets try to do some fun
			// translations to python standard methods
			bool call = false;
			if (string(attr) == "toString") attr = "__str__";          else
			if (string(attr) == "length")   call = (attr = "__len__");

			if (call)
				rval = PyObject_CallMethod(pyobj, (char*) attr, NULL);
			else
				rval = PyObject_GetAttrString(pyobj, attr);
			if (PyErr_Occurred()) {
				PyErr_Clear();
				return obj.newUndefined();
			}
		}
		return value_from_pyobject(obj, rval);
	}

	virtual Value set(Value& obj, long idx, Value& value) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject* val = pyobject_from_value(value);
		if (!val) return obj.newBool(false);

		PyObject* key = PyLong_FromLong(idx);
		if (!key) {
			Py_XDECREF(val);
			return obj.newBool(false);
		}

		bool rval = PyObject_SetItem(pyobj, key, val) != -1;
		Py_XDECREF(val);
		Py_XDECREF(key);
		if (PyErr_Occurred())
			PyErr_Clear();
		return obj.newBool(rval);
	}

	virtual Value set(Value& obj, string name, Value& value) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject* val = pyobject_from_value(value);
		if (!val) return obj.newBool(false);

		bool rval = PyObject_SetAttrString(pyobj, name.c_str(), val) != -1;
		Py_XDECREF(val);
		if (PyErr_Occurred())
			PyErr_Clear();
		return obj.newBool(rval);
	}

	virtual Value enumerate(Value& obj) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject* iter = PyObject_GetIter(pyobj);
		if (PyErr_Occurred()) PyErr_Clear();
		if (!iter) return obj.newArray();

		PyObject* item;
		vector<Value> items;
		while ((item = PyIter_Next(iter)))
			items.push_back(value_from_pyobject(obj, item));
		Py_XDECREF(iter);

		return obj.newArray(items);
	}
};

class PythonCallableClass : public PythonObjectClass {
public:
	virtual Class::Flags getFlags () {
		return Class::FlagAll;
	}

	virtual Value call     (Value& obj, vector<Value> args) {
		PyObject* pyobj = (PyObject*) obj.getPrivate("python");
		assert(pyobj);

		PyObject *argt = PyTuple_New(args.size());
		for (unsigned int i=0 ; i < args.size() ; i++) {
			PyObject* val = pyobject_from_value(args[i]);
			if (PyTuple_SetItem(argt, i, val) == -1) {
				Py_XDECREF(argt);
				return obj.newUndefined();
			}
		}

		PyObject *res = PyObject_Call(pyobj, argt, NULL);
		Py_XDECREF(argt);
		if (!res) {
			if (PyErr_Occurred()) {
				PyObject *type=NULL, *val=NULL, *tb=NULL;
				PyErr_Fetch(&type, &val, &tb);
				if (val) {
					Py_XINCREF(val);
					return value_from_pyobject(obj, val).toException();
				}
			}
			return obj.newUndefined();
		}
		return value_from_pyobject(obj, res);
	}

	virtual Value callNew  (Value& obj, vector<Value> args) {
		return obj.newString("Python has no concept of constructors!").toException();
	}
};

static int append_striter_to_strvect(PyObject* obj, vector<string>* vect, int offset=0) {
	PyObject* iter = PyObject_GetIter(obj);
	if (!iter) return false;

	for (PyObject* item ; (item = PyIter_Next(iter)) ; ) {
		if (offset == 0) {
			if (PyString_Check(item))
				vect->push_back(PyString_AsString(item));
		} else
			offset--;
		Py_DECREF(item);
	}
	Py_XDECREF(iter);
	return true;
}

static int append_valiter_to_valvect(PyObject* obj, vector<Value>* vect, Value value, int offset=0) {
	PyObject* iter = PyObject_GetIter(obj);
	if (!iter) return false;

	for (PyObject* item ; (item = PyIter_Next(iter)) ; ) {
		if (offset == 0)
			vect->push_back(value_from_pyobject(value, item));
		else
			offset--;
		Py_DECREF(item);
	}
	Py_XDECREF(iter);
	return true;
}

static PyObject* pyobject_from_value_exc(Value val) {
	if (val.isException()) {
		PyErr_SetString(NatusException, val.toString().c_str());
		return NULL;
	}

	return pyobject_from_value(val);
}

static PyObject *Value_new(struct _typeobject *type, PyObject *args, PyObject *keywords) {
	natus_Value* self = (natus_Value*) type->tp_alloc(type, 0);
	if (self) new (&self->value) Value(); // Create a new object in already allocated memory
	return (PyObject*) self;
}

static void Value_dealloc(PyObject* self) {
	((natus_Value*) self)->value.~Value(); // Manually call the destructor
	self->ob_type->tp_free(self);
}

PyObject *Value_getattr(PyObject *self, PyObject* attr) {
	PyObject* obj = PyObject_GenericGetAttr(self, attr);
	if (obj) return obj;
	PyErr_Clear();
	Value val = ((natus_Value*) self)->value.get(PyString_AsString(attr));
	if (!val.isUndefined())
		return pyobject_from_value(val);
	PyErr_SetString(PyExc_AttributeError, "Attribute not found!");
	return NULL;
}

int Value_setattr(PyObject *self, char* attr, PyObject* value) {
	natus_Value* vself = (natus_Value*) self;

	if (!value) {
		if (!vself->value.del(attr))
			goto error;
		return 0;
	}

	Py_XINCREF(value);
	if (vself->value.set(attr, value_from_pyobject(vself->value, value)))
		return 0;

	error:
		PyErr_SetString(PyExc_AttributeError, "Unable to set attribute!");
		return -1;
}

static PyObject* Value_getitem(PyObject *self, PyObject *item) {
	char *name = NULL;
	long  indx = 0;

	if (PyString_Check(item))
		name = PyString_AsString(item);
	else if (PyLong_Check(item))
		indx = PyLong_AsLong(item);
	else if (PyInt_Check(item))
		indx = PyInt_AsLong(item);
	else {
		PyErr_SetString(PyExc_KeyError, "Key must be int, long or string!");
		return NULL;
	}

	Value val;
	if (name)
		val = ((natus_Value*) self)->value.get(name);
	else
		val = ((natus_Value*) self)->value.get(indx);

	if (!val.isUndefined())
		return pyobject_from_value(val);

	PyErr_SetObject(PyExc_KeyError, item);
	return NULL;
}

static int Value_setitem(PyObject *self, PyObject *item, PyObject *value) {
	natus_Value* vself = (natus_Value*) self;
	char *name = NULL;
	long  indx = 0;
	bool res = false;

	if (PyString_Check(item))
		name = PyString_AsString(item);
	else if (PyLong_Check(item))
		indx = PyLong_AsLong(item);
	else if (PyInt_Check(item))
		indx = PyInt_AsLong(item);
	else {
		PyErr_SetString(PyExc_KeyError, "Key must be int, long or string!");
		return -1;
	}

	if (!value) {
		if (name)
			res = vself->value.del(name);
		else
			res = vself->value.del(indx);
		if (res) return 0;
		PyErr_SetString(PyExc_ValueError, "Unable to delete item!");
		return -1;
	}

	Py_XINCREF(value);
	if (name)
		res = vself->value.set(name, value_from_pyobject(vself->value, value));
	else
		res = vself->value.set(indx, value_from_pyobject(vself->value, value));

	if (res) return 0;
	PyErr_SetString(PyExc_ValueError, "Unable to set item!");
	return -1;
}

static PyMappingMethods ValueMappingMethods = {
	NULL,
	Value_getitem,
	Value_setitem
};

static PyObject* Value_getGlobal(PyObject* self, PyObject* args) {
	return pyobject_from_value_exc(((natus_Value*) self)->value.getGlobal());
}

static PyObject* Value_evaluate(PyObject* self, PyObject* args) {
	const char* jscript  = NULL;
	const char* filename = NULL;
	int lineno = 0;
	PyObject* shift = NULL;
	if (!PyArg_ParseTuple(args, "s|siO", &jscript, &filename, &lineno, &shift))
		return NULL;

	bool doshift = shift && PyObject_IsTrue(shift);
	Value val = ((natus_Value*) self)->value.evaluate(jscript, filename ? filename : "", lineno, doshift);
	return pyobject_from_value_exc(val);
}

static PyObject* Value_call(PyObject* self, PyObject* args) {
	vector<Value> params;
	PyObject* funcOrName = PyTuple_GetItem(args, 0);
	append_valiter_to_valvect(args, &params, ((natus_Value*) self)->value, 1);

	if (funcOrName && PyObject_IsInstance(funcOrName, (PyObject*) &natus_ValueType))
		return pyobject_from_value_exc(((natus_Value*) self)->value.call(((natus_Value*) funcOrName)->value, params));

	else if (funcOrName && PyString_Check(funcOrName)) {
		const char* name = PyString_AsString(funcOrName);
		return pyobject_from_value_exc(((natus_Value*) self)->value.call(name, params));
	}

	PyErr_SetString(NatusException, "The first paramater MUST be a callable Natus Value or the name of the function on the current value!");
	return NULL;
}

static PyObject* Value_callNew(PyObject* self, PyObject* args) {
	vector<Value> params;
	append_valiter_to_valvect(args, &params, ((natus_Value*) self)->value, 1);
	return pyobject_from_value_exc(((natus_Value*) self)->value.callNew(params));
}

static PyObject* Value_require(PyObject* self, PyObject* args) {
	const char* name   = NULL;
	const char* reldir = NULL;
	vector<string> path;

	if (!PyArg_ParseTuple(args, "ssO&", &name, &reldir, append_striter_to_strvect, &path))
		return NULL;

	ModuleLoader* ml = ModuleLoader::getModuleLoader(((natus_Value*) self)->value);
	if (!ml) {
		PyErr_SetString(PyExc_ImportError, "Unable to find ModuleLoader!");
		return NULL;
	}

	return pyobject_from_value_exc(ml->require(name, reldir, path));
}

static PyMethodDef Value_methods[] = {
    {"getGlobal", Value_getGlobal, METH_NOARGS, "Fetches the global Value" },
    {"evaluate", Value_evaluate, METH_VARARGS, "Evaluates javascript, returns the result." },
    {"call", Value_call, METH_VARARGS, "Calls a natus function/object." },
    {"callNew", Value_callNew, METH_VARARGS, "Creates a new object from a natus function/object." },
    {"require", Value_require, METH_VARARGS, "Loads a natus module." },
    {NULL}
};

static PyObject *Engine_new(struct _typeobject *type, PyObject *args, PyObject *keywords) {
	natus_Engine* self = (natus_Engine*) type->tp_alloc(type, 0);
	if (self) new (&self->engine) Engine(); // Create a new object in already allocated memory
	return (PyObject*) self;
}

static void Engine_dealloc(PyObject* self) {
	((natus_Engine*) self)->engine.~Engine(); // Manually call the destructor
	self->ob_type->tp_free(self);
}

static int Engine_init(natus_Engine* self, PyObject* args, PyObject* kwds) {
	const char* name_or_path=NULL;
	if (!PyArg_ParseTuple(args, "|s", &name_or_path))
		return -1;

	if (!self->engine.initialize(name_or_path)) {
		PyErr_SetString(NatusException, "Unable to initialize Javascript engine!");
		return -1;
	}
    return 0;
}

static PyObject* Engine_newGlobal(PyObject* self, PyObject* args) {
	return pyobject_from_value_exc(((natus_Engine*) self)->engine.newGlobal());
}

static PyMethodDef Engine_methods[] = {
    {"newGlobal", Engine_newGlobal, METH_VARARGS, "Creates a new global Value" },
    {NULL}
};

static PyObject *Engine_name(natus_Engine *self, void *huh) {
	return PyString_FromString(self->engine.getName().c_str());
}

static PyGetSetDef Engine_getseters[] = {
    {(char*) "name", (getter) Engine_name, NULL, (char*) "Name of the backend engine", NULL },
    {NULL, NULL, NULL, NULL, NULL}
};

static void pyobj_finalize(void *po) {
	if (!po) return;
	Py_XDECREF((PyObject *) po);
}

static PyObject* pyobject_from_value(Value val) {
	PyObject *obj = NULL;

	if (val.isArray()) {
		long len = val.get("length").toLong();
		obj = PyList_New(len);
		for (--len ; len >= 0 ; len--)
			PyList_SetItem(obj, len, pyobject_from_value(val.get(len)));
	} else if (val.isBool()) {
		obj = val.toBool() ? Py_True : Py_False;
		Py_XINCREF(obj);
	} else if (val.isNull() || val.isUndefined()) {
		obj = Py_None;
		Py_XINCREF(obj);
	} else if (val.isNumber()) {
		obj = PyFloat_FromDouble(val.toDouble());
	} else if (val.isFunction() || val.isObject()) {
		obj = (PyObject*) val.getPrivate("python");
		if (!obj) {
			obj = Value_new(&natus_ValueType, NULL, NULL);
			if (obj) {
				obj->ob_refcnt = 0; // We will increase below
				((natus_Value*) obj)->value = val;
			} else
				obj = Py_None;
		}
		Py_XINCREF(obj);
	} else if (val.isString()) {
		obj = PyString_FromString(val.toString().c_str());
	} else
		assert(false);
	return obj;
}

Value value_from_pyobject(Value val, PyObject *obj) {
	Value ret = val.newUndefined();

	if (obj && PyUnicode_Check(obj)) {
		PyObject *tmp = obj;
		obj = PyUnicode_AsUTF8String(obj);
		Py_XDECREF(tmp);
	}

	if (!obj)
		ret = val.newUndefined();
	else if (Py_None == obj)
		ret = val.newNull();
	else if (PyBool_Check(obj))
		ret = val.newBool(PyObject_IsTrue(obj));
	else if (PyInt_Check(obj))
		ret = val.newNumber(PyInt_AsLong(obj));
	else if (PyLong_Check(obj))
		ret = val.newNumber(PyLong_AsDouble(obj));
	else if (PyFloat_Check(obj))
		ret = val.newNumber(PyFloat_AsDouble(obj));
	else if (PyString_Check(obj))
		ret = val.newString(PyString_AsString(obj));
	else if (PyObject_IsInstance(obj, (PyObject*) &natus_ValueType))
		ret = ((natus_Value*) obj)->value;
	else {
		if (PyCallable_Check(obj))
			ret = val.newObject(new PythonCallableClass());
		else
			ret = val.newObject(new PythonObjectClass());
		ret.setPrivate("python", obj, pyobj_finalize);
		Py_XINCREF(obj);
	}

	Py_XDECREF(obj);
	return ret;
}

void readyNatusTypes() {
	// Prepare the Engine type
	natus_EngineType.tp_basicsize = sizeof(natus_Engine);
	natus_EngineType.tp_doc       = "A Natus Engine";
	natus_EngineType.tp_flags     = Py_TPFLAGS_DEFAULT;
	natus_EngineType.tp_new       = Engine_new;
	natus_EngineType.tp_dealloc   = Engine_dealloc;
	natus_EngineType.tp_init      = (initproc) Engine_init;
	natus_EngineType.tp_methods   = Engine_methods;
	natus_EngineType.tp_getset    = Engine_getseters;
	if (PyType_Ready(&natus_EngineType) < 0)
		return;

	// Prepare the Value type
	natus_ValueType.tp_basicsize  = sizeof(natus_Value);
	natus_ValueType.tp_doc        = "A Natus Value";
	natus_ValueType.tp_flags      = Py_TPFLAGS_DEFAULT;
	natus_ValueType.tp_new        = NULL; // Don't allow instantiation from within Python
	natus_ValueType.tp_dealloc    = Value_dealloc;
	natus_ValueType.tp_methods    = Value_methods;
	natus_ValueType.tp_getattro   = Value_getattr;
	natus_ValueType.tp_setattr    = Value_setattr;
	natus_ValueType.tp_as_mapping = &ValueMappingMethods;
	if (PyType_Ready(&natus_ValueType) < 0)
		return;

	// Our exception
	NatusException = PyErr_NewException((char*) "natus.NatusException", NULL, NULL);
}
