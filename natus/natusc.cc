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

#include <cstring>
#include <cstdlib>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natusc.h"
#include "natus.h"
using namespace natus;

#define NFUNCPRIV "natus_cNativeFunction"

struct _ntEngine {
	Engine engine;
};

struct _ntValue {
	Value value;
};

struct _ntModuleLoader {
	ModuleLoader ml;
};

class NatusCClass : public Class {
private:
	ntClass* ccls;
	Class::Flags flags;
public:
	NatusCClass(ntClass* ccls) {
		ccls = ccls;
		flags = Class::FlagNone;
		if (ccls->del_prop)  flags = (Class::Flags) (flags | Class::FlagDeleteProperty);
		if (ccls->del_item)  flags = (Class::Flags) (flags | Class::FlagDeleteItem);
		if (ccls->get_prop)  flags = (Class::Flags) (flags | Class::FlagGetProperty);
		if (ccls->get_item)  flags = (Class::Flags) (flags | Class::FlagGetItem);
		if (ccls->set_prop)  flags = (Class::Flags) (flags | Class::FlagSetProperty);
		if (ccls->set_item)  flags = (Class::Flags) (flags | Class::FlagSetItem);
		if (ccls->enumerate) flags = (Class::Flags) (flags | Class::FlagEnumerate);
		if (ccls->call)      flags = (Class::Flags) (flags | Class::FlagCall);
		if (ccls->call_new)  flags = (Class::Flags) (flags | Class::FlagNew);
	}

	virtual Class::Flags getFlags () {
		return flags;
	}

	virtual Value        del      (Value& obj, string name) {
		if (!ccls->del_prop) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->del_prop((ntValue*) &obj, name.c_str());
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        del      (Value& obj, long idx) {
		if (!ccls->del_item) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->del_item((ntValue*) &obj, idx);
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        get      (Value& obj, string name) {
		if (!ccls->get_prop) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->get_prop((ntValue*) &obj, name.c_str());
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        get      (Value& obj, long idx) {
		if (!ccls->get_item) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->get_item((ntValue*) &obj, idx);
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        set      (Value& obj, string name, Value& value) {
		if (!ccls->set_prop) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->set_prop((ntValue*) &obj, name.c_str(), (ntValue*) &value);
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        set      (Value& obj, long idx, Value& value) {
		if (!ccls->set_item) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->set_item((ntValue*) &obj, idx, (ntValue*) &value);
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        enumerate(Value& obj) {
		if (!ccls->enumerate) return obj.newUndefined().toException();
		Value* tmp = (Value*) ccls->enumerate((ntValue*) &obj);
		Value  ret = *tmp;
		delete tmp;
		return ret;
	}

	virtual Value        call     (Value& obj, vector<Value> args) {
		if (!ccls->call) return obj.newUndefined().toException();
		ntValue** argv = new ntValue*[args.size()+1];
		for (size_t i=0 ; i < args.size() ; i++)
			argv[i] = (ntValue*) &args[i];
		argv[args.size()] = NULL;
		Value* tmp = (Value*) ccls->call((ntValue*) &obj, argv);
		Value  ret = *tmp;
		delete[] argv;
		delete tmp;
		return ret;
	}

	virtual Value        callNew  (Value& obj, vector<Value> args) {
		if (!ccls->call_new) return obj.newUndefined().toException();
		ntValue** argv = new ntValue*[args.size()+1];
		for (size_t i=0 ; i < args.size() ; i++)
			argv[i] = (ntValue*) &args[i];
		argv[args.size()] = NULL;
		Value* tmp = (Value*) ccls->call_new((ntValue*) &obj, argv);
		Value  ret = *tmp;
		delete[] argv;
		delete tmp;
		return ret;
	}

	virtual ~NatusCClass() {
		if (ccls)
			ccls->free(ccls);
	}
};

Value cNativeFunction(Value& ths, Value& fnc, vector<Value>& arg) {
	ntNativeFunction func = (ntNativeFunction) fnc.getPrivate(NFUNCPRIV);
	Value** args = new Value*[arg.size()+1];
	int i=0;
	for (vector<Value>::size_type i=0 ; i < arg.size() ; i++)
		args[i] = &arg[i];
	args[i] = 0;

	Value* tmp = (Value*) func((ntValue*) &ths, (ntValue*) &fnc, (ntValue**) args);
	Value  ret = *tmp;
	delete args;
	delete tmp;
	return ret;
}

struct cRequireMisc {
	ntRequireFunction func;
	ntFreeFunction    free;
	void*             misc;
	~cRequireMisc() {
		if (free && misc)
			free(misc);
	}
};

static Value cRequireFunction(Value& module, string& name, string& reldir, vector<string>& path, cRequireMisc* crhs) {
	const char **vpath = (const char**) malloc(sizeof(char *) * (path.size()+1));
	if (!vpath) return module.newUndefined();
	memset(vpath, 0, sizeof(char *) * (path.size()+1));
	int i=0;
	for (vector<string>::iterator it=path.begin() ; it != path.end() ; it++)
		vpath[i++] = it->c_str();
	ntValue* res = crhs->func((ntValue*) &module, name.c_str(), reldir.c_str(), vpath, crhs->misc);
	delete[] vpath;
	if (res == NULL)
		return module.newUndefined();
	Value ret = res->value;
	nt_free(res);
	return ret;
}

static void cRequireFree(cRequireMisc* crhs) {
	delete crhs;
}

ntEngine         *nt_engine_init(const char *name_or_path) {
	_ntEngine* e = new _ntEngine;
	if (e->engine.initialize(name_or_path))
		return e;
	delete e;
	return NULL;
}

char             *nt_engine_name(ntEngine *engine) {
	return strdup(engine->engine.getName().c_str());
}

#define to_ntValue(x) (_ntValue*) new Value(x)

ntValue          *nt_engine_new_global(ntEngine *engine) {
	return to_ntValue(engine->engine.newGlobal());
}

void              nt_engine_free(ntEngine *engine) {
	delete engine;
}

void              nt_free(ntValue *val) {
	delete (Value*) val;
}

ntValue          *nt_new_bool(const ntValue *ctx, bool b) {
	return to_ntValue(ctx->value.newBool(b));
}

ntValue          *nt_new_number(const ntValue *ctx, double n) {
	return to_ntValue(ctx->value.newNumber(n));
}

ntValue          *nt_new_string(const ntValue *ctx, const char *string) {
	return to_ntValue(ctx->value.newString(string));
}

ntValue          *nt_new_array(const ntValue *ctx, ntValue **array) {
	vector<Value> args;
	for (int i=0 ; array && array[i] ; i++)
		args.push_back(array[i]->value);
	return to_ntValue(ctx->value.newArray(args));
}

ntValue          *nt_new_function(const ntValue *ctx, ntNativeFunction func) {
	ntValue* v = to_ntValue(ctx->value.newFunction(cNativeFunction));
	v->value.setPrivate(NFUNCPRIV, (void*) func);
	return v;
}

ntValue          *nt_new_object(const ntValue *ctx, ntClass* cls) {
	if (!cls)
		return to_ntValue(ctx->value.newObject());
	return to_ntValue(ctx->value.newObject(new NatusCClass(cls)));
}

ntValue          *nt_new_null(const ntValue *ctx) {
	return to_ntValue(ctx->value.newNull());
}

ntValue          *nt_new_undefined(const ntValue *ctx) {
	return to_ntValue(ctx->value.newUndefined());
}

ntValue          *nt_get_global(const ntValue *ctx) {
	return to_ntValue(ctx->value.getGlobal());
}

void              nt_get_context(const ntValue *ctx, void **context, void **value) {
	ctx->value.getContext(context, value);
}

bool              nt_is_global(const ntValue *val) {
	return val->value.isGlobal();
}

bool              nt_is_exception(const ntValue *val) {
	return val->value.isException();
}

bool              nt_is_oom(const ntValue *val) {
	return val->value.isOOM();
}

bool              nt_is_array(const ntValue *val) {
	return val->value.isArray();
}

bool              nt_is_bool(const ntValue *val) {
	return val->value.isBool();
}

bool              nt_is_function(const ntValue *val) {
	return val->value.isFunction();
}

bool              nt_is_null(const ntValue *val) {
	return val->value.isNull();
}

bool              nt_is_number(const ntValue *val) {
	return val->value.isNumber();
}

bool              nt_is_object(const ntValue *val) {
	return val->value.isObject();
}

bool              nt_is_string(const ntValue *val) {
	return val->value.isString();
}

bool              nt_is_undefined(const ntValue *val) {
	return val->value.isUndefined();
}

bool              nt_to_bool(const ntValue *val) {
	return val->value.toBool();
}

double            nt_to_double(const ntValue *val) {
	return val->value.toDouble();
}

void              nt_to_exception(ntValue *val) {
	val->value = val->value.toException();
}

int               nt_to_int(const ntValue *val) {
	return val->value.toInt();
}

long              nt_to_long(const ntValue *val) {
	return val->value.toLong();
}

char             *nt_to_string(const ntValue *val) {
	return strdup(val->value.toString().c_str());
}

char            **nt_to_string_vector(const ntValue *val) {
	vector<string> sv = val->value.toStringVector();
	char **ret = (char **) malloc(sizeof(char *) * (sv.size()+1));
	if (!ret) return NULL;
	memset(ret, 0, sizeof(char *) * (sv.size()+1));
	int i=0;
	for (vector<string>::iterator it=sv.begin() ; it != sv.end() ; it++)
		ret[i++] = strdup(it->c_str());
	return ret;
}

bool              nt_del_property(ntValue *val, const char *name) {
	return val->value.del(name);
}

bool              nt_del_item(ntValue *val, long idx) {
	return val->value.del(idx);
}

ntValue          *nt_get_property(const ntValue *val, const char *name) {
	return to_ntValue(val->value.get(name));
}

ntValue          *nt_get_item(const ntValue *val, long idx) {
	return to_ntValue(val->value.get(idx));
}

bool              nt_has_property(const ntValue *val, const char *name) {
	return val->value.has(name);
}

bool              nt_has_item(const ntValue *val, long idx) {
	return val->value.has(idx);
}

bool              nt_set_property(ntValue *val, const char *name, ntValue *value) {
	return val->value.set(name, value->value);
}

bool              nt_set_property_attr(ntValue *val, const char *name, ntValue *value, ntPropAttrs attrs) {
	return val->value.set(name, value->value, (Value::PropAttrs) attrs);
}

bool              nt_set_property_full(ntValue *val, const char *name, ntValue *value, ntPropAttrs attrs, bool makeParents) {
	return val->value.set(name, value->value, (Value::PropAttrs) attrs, makeParents);
}

bool              nt_set_item(ntValue *val, long idx, ntValue *value) {
	return val->value.set(idx, value->value);
}

char            **nt_enumerate(const ntValue *val) {
	set<string> enums = val->value.enumerate();
	char **ret = (char**) malloc(sizeof(char *) * (enums.size()+1));
	if (!ret) return NULL;
	memset(ret, 0, sizeof(char *) * (enums.size()+1));
	int i=0;
	for (set<string>::iterator it=enums.begin() ; it != enums.end() ; it++)
		ret[i++] = strdup(it->c_str());
	return ret;
}

bool              nt_set_private(ntValue *val, const char *key, void *priv) {
	return val->value.setPrivate(key, priv);
}

bool              nt_set_private_full(ntValue *val, const char *key, void *priv, ntFreeFunction free) {
	return val->value.setPrivate(key, priv, free);
}

bool              nt_set_private_value(ntValue *val, const char *key, ntValue* priv) {
	return val->value.setPrivate(key, priv->value);
}

void*             nt_get_private(const ntValue *val, const char *key) {
	return val->value.getPrivate(key);
}

ntValue          *nt_get_private_value(const ntValue *val, const char *key) {
	return to_ntValue(val->value.getPrivateValue(key));
}

ntValue          *nt_evaluate(ntValue *ths, const char *jscript, const char *filename, unsigned int lineno) {
	return nt_evaluate_full(ths, jscript, filename, lineno, false);
}

ntValue          *nt_evaluate_full(ntValue *ths, const char *jscript, const char *filename, unsigned int lineno, bool shift) {
	return to_ntValue(ths->value.evaluate(jscript, filename ? filename : "", lineno, shift));
}

ntValue          *nt_call(ntValue *ths, ntValue *func, ntValue **args) {
	vector<Value> vargs;
	for (int i=0 ; args && args[i] ; i++)
		vargs.push_back(args[i]->value);
	return to_ntValue(ths->value.call(func->value, vargs));
}

ntValue          *nt_call_name(ntValue *ths, const char *name, ntValue **args) {
	vector<Value> vargs;
	for (int i=0 ; args && args[i] ; i++)
		vargs.push_back(args[i]->value);
	return to_ntValue(ths->value.call(name, vargs));
}

ntValue          *nt_new(ntValue *func, ntValue **args) {
	vector<Value> vargs;
	for (int i=0 ; args && args[i] ; i++)
		vargs.push_back(args[i]->value);
	return to_ntValue(func->value.callNew(vargs));
}

ntValue          *nt_new_name(ntValue *ths, const char *name, ntValue **args) {
	vector<Value> vargs;
	for (int i=0 ; args && args[i] ; i++)
		vargs.push_back(args[i]->value);
	return to_ntValue(ths->value.callNew(name, vargs));
}

ntModuleLoader   *nt_ml_init(ntValue *ctx, const char *config) {
	ModuleLoader* ml = new ModuleLoader(ctx->value);
	if (ml->initialize(config ? config : "{}").toBool()) {
		delete ml;
		return NULL;
	}
	return (ntModuleLoader*) ml;
}

int               nt_ml_add_require_hook(ntModuleLoader *ml, bool post, ntRequireFunction func, void* misc, ntFreeFunction free) {
	cRequireMisc* crhs = new cRequireMisc;
	crhs->func = func;
	crhs->free = free;
	crhs->misc = misc;

	return ml->ml.addRequireHook(post, (RequireFunction) cRequireFunction, crhs, (FreeFunction) cRequireFree);
}

void              nt_ml_del_require_hook(ntModuleLoader *ml, int id) {
	ml->ml.delRequireHook(id);
}

int               nt_ml_add_origin_matcher(ntModuleLoader *ml, ntOriginMatcher func, void* misc, ntFreeFunction free) {
	return ml->ml.addOriginMatcher(func, misc, free);
}

void              nt_ml_del_origin_matcher(ntModuleLoader *ml, int id) {
	ml->ml.delOriginMatcher(id);
}

ntValue          *nt_ml_require(const ntModuleLoader *ml, const char *name, const char *reldir, const char **path) {
	vector<string> vpath;
	for (int i=0 ; path && path[i] ; i++)
		vpath.push_back(path[i]);
	return to_ntValue(ml->ml.require(name, reldir, vpath));
}

bool              nt_ml_origin_permitted(const ntModuleLoader *ml, const char *uri) {
	return ml->ml.originPermitted(uri);
}

void              nt_ml_free(ntModuleLoader *ml) {
	delete (ModuleLoader*) ml;
}

ntValue          *nt_new_exception(const ntValue *ctx, const char *type, const char *message) {
	return to_ntValue(throwException(ctx->value, type, message));
}

ntValue          *nt_new_exception_code(const ntValue *ctx, const char *type, const char *message, long code) {
	return to_ntValue(throwException(ctx->value, type, message, code));
}

ntValue          *nt_new_exception_errno(const ntValue *ctx, int errorno) {
	return to_ntValue(throwException(ctx->value, errorno));
}

ntValue          *nt_check_arguments(const ntValue *ctx, ntValue **args, const char *fmt) {
	vector<Value> vargs;
	for (int i=0 ; args && args[i] ; i++)
		vargs.push_back(args[i]->value);
	return to_ntValue(checkArguments(ctx->value, vargs, fmt));
}
