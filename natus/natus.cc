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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "engine.h"

#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <stack>
#include <libgen.h>

#include <iostream>

#define  _str(s) # s
#define __str(s) _str(s)

#define MODSUFFIX  ".so"
#define SCRSUFFIX  ".js"
#define PKGSUFFIX  "__init__" SCRSUFFIX
#define URIPREFIX  "file://"
#define NFSUFFIX   " not found!"

namespace natus {

struct EngineInternal {
	EngineSpec* espec;
	void*       engine;
	void*       dll;
};

struct RequireInternal {
	vector<string> whitelist;
	vector<string> path;
	stack<string>  exec;

	static void free(void *ri) {
		delete ((RequireInternal*) ri);
	}

	RequireInternal(vector<string> path, vector<string> whitelist) {
		this->exec      = stack<string>();
		this->path      = vector<string>();
		this->whitelist = vector<string>();

		if (whitelist.size() > 0 && path.size() > 0) {
			for (vector<string>::iterator i=path.begin() ; i < path.end() ; i++)
				this->path.push_back(*i);
			for (vector<string>::iterator i=whitelist.begin() ; i < whitelist.end() ; i++)
				this->whitelist.push_back(*i);
		}
	}

	void push(string filename) {
		char *tmp = strdup(filename.c_str());
		filename = dirname(tmp);
		std::free(tmp);
		exec.push(filename);
	}

	string peek() {
		if (exec.size() == 0)
			return string();
		return exec.top();
	}

	string pop() {
		string tmp = exec.top();
		exec.pop();
		return tmp;
	}
};

static bool do_load_file(string filename, bool reqsym, EngineSpec** enginespec, void** engine, void** dll) {
	void *dll_ = dlopen(filename.c_str(), RTLD_LAZY | RTLD_LOCAL);
	if (!dll_) {
		//printf("%s -- %s\n", filename.c_str(), dlerror());
		return false;
	}

	EngineSpec* module = (EngineSpec*) dlsym(dll_, __str(NATUS_ENGINE_));
	if (!module || module->vers != NATUS_ENGINE_VERSION || !module->init) {
		dlclose(dll_);
		return false;
	}

	if (module->symb && reqsym) {
		void *tmp = dlopen(NULL, 0);
		if (!tmp || !dlsym(tmp, module->symb)) {
			if (tmp)
				dlclose(tmp);
			dlclose(dll_);
			return false;
		}
		dlclose(tmp);
	}

	void *eng = NULL;
	if (!(eng = module->init())) {
		dlclose(dll_);
		return false;
	}

	*dll = dll_;
	*engine = eng;
	*enginespec = module;
	return true;
}

static bool do_load_dir(string dirname, bool reqsym, EngineSpec** enginespec, void** engine, void** dll) {
	bool res = false;
	DIR *dir = opendir(dirname.c_str());
	if (!dir)
		return NULL;

	struct dirent *ent = NULL;
	while ((ent = readdir(dir))) {
		if (string(".") == ent->d_name || string("..") == ent->d_name) continue;
		string filename = string(ent->d_name);
		string   suffix = string(MODSUFFIX);
		size_t     flen = filename.length();
		size_t     slen = suffix.length();
		if (flen < slen || filename.substr(flen-slen) != suffix)
			continue;

		if ((res = do_load_file(dirname + "/" + filename, reqsym, enginespec, engine, dll)))
			break;
	}

	closedir(dir);
	return res;
}

static Value require_file(Value *ctx, string abspath, string id, bool sandbox) {
	// Stat our file
	struct stat st;
	if (stat(abspath.c_str(), &st))
		return ctx->newUndefined().toException();

	// Setup our import environment
	Value::PropAttrs fixed = (Value::PropAttrs) (Value::DontDelete | Value::ReadOnly);
	Value base    = ctx->newObject();
	Value exports = ctx->newObject();
	Value module  = ctx->newObject();
	if (base.isUndefined() || exports.isUndefined() || module.isUndefined()) return ctx->newUndefined().toException();
	if (!base.set("exports", exports, fixed))                                return ctx->newUndefined().toException();
	if (!base.set("module", module, fixed))                                  return ctx->newUndefined().toException();
	if (!module.set("id", ctx->newString(id), fixed))                        return ctx->newUndefined().toException();
	if (!base.set("require", ctx->get("require"), fixed))                    return ctx->newUndefined().toException();
	if (!sandbox)
		if (!module.set("uri", ctx->newString(URIPREFIX+abspath), fixed))
			return ctx->newUndefined().toException();

	// If we have a binary module -- .so/.dll
	if (abspath.substr(abspath.length() - string(MODSUFFIX).length(), string(MODSUFFIX).length()) == MODSUFFIX) {
		void *dll = dlopen(abspath.c_str(), RTLD_NOW | RTLD_GLOBAL);
		if (!dll)
			return ctx->newString(dlerror()).toException();


		RequireInternal* ri = ((RequireInternal*) ctx->getGlobal().get("require").getPrivate("natus_require"));
		if (ri) ri->push(abspath);
		bool (*load)(Value&) = (bool (*)(Value&)) dlsym(dll, "natus_require");
		if (!load || !load(base)) {
			if (ri) ri->pop();
			dlclose(dll);
			return ctx->newString("Module initialization failed!").toException();
		}
		if (ri) ri->pop();
		// Leak the dll, maybe tackle this later?

		exports.set("module", module, fixed);
		return exports;
	}

	// If we have a javascript text module
	ifstream file(abspath.c_str());
	if (!file.is_open()) return ctx->newUndefined().toException();
	char *jscript = new char[st.st_size + 1];
	file.read(jscript, st.st_size);
	if (!file.good() && !file.eof()) {
		file.close();
		delete jscript;
		return ctx->newString("Error reading file!").toException();
	}
	file.close();

	Value res = ctx->evaluate(jscript, abspath, 0, true);
	delete[] jscript;
	if (res.isException())
		return res;
	exports.set("module", module, fixed);
	return exports;
}

static Value require_js(Value& ths, Value& fnc, vector<Value>& arg) {
	// Get the private value
	void* msc = fnc.getPrivate("natus_require");

	// Get the required name argument
	if (arg.size() < 1 || !arg[0].isString())
		return ths.newString("Invalid module name!").toException();
	string module = arg[0].toString();

	// Build our path and whitelist
	vector<string> path;
	vector<string> whitelist;
	if (msc && ((RequireInternal*) msc)->path.size() > 0) {
		path      = ((RequireInternal*) msc)->path;
		whitelist = ((RequireInternal*) msc)->whitelist;
	} else {
		path      = fnc.get("paths").toStringVector();
		whitelist = vector<string>();
	}

	// Sanity check
	if (path.size() == 0)
		return ths.newString("No import paths found!").toException();
	if (whitelist.size() > 0) {
		bool cleared = false;
		for (unsigned int i=0 ; !cleared && i < whitelist.size() ; i++)
			cleared = whitelist[i] == module;
		if (!cleared)
			return ths.newString("Module not in the whitelist!").toException();
	}

	return ths.require(module, ((RequireInternal*) msc)->peek(), path);
}

Class::Flags Class::getFlags() {
	return Class::FlagNone;
}

Value Class::del(Value& obj, string name) {
	return obj.newUndefined();
}

Value Class::del(Value& obj, long idx) {
	return obj.newUndefined();
}

Value Class::get(Value& obj, string name) {
	return obj.newUndefined();
}

Value Class::get(Value& obj, long idx) {
	return obj.newUndefined();
}

Value Class::set(Value& obj, string name, Value& value) {
	return obj.newUndefined();
}

Value Class::set(Value& obj, long idx, Value& value) {
	return obj.newUndefined();
}

Value Class::enumerate(Value& obj) {
	vector<Value> args = vector<Value>();
	return obj.newUndefined();
}

Value Class::call(Value& obj, vector<Value> args) {
	return obj.newUndefined();
}

Value Class::callNew(Value& obj, vector<Value> args) {
	return obj.newUndefined();
}

Class::~Class() {}

Engine::Engine() {
	this->internal = NULL;
}

Engine::~Engine() {
	EngineInternal* ei = ((EngineInternal*) internal);
	if (!ei) return;
	if (ei->espec && ei->espec->free && ei->engine)
		ei->espec->free(ei->engine);
	if (ei->dll)
		dlclose(ei->dll);
	delete ei;
}

bool Engine::initialize(const char* name_or_path) {
	internal = new EngineInternal();

	bool result = false;
	if (name_or_path) {
		result = do_load_file(name_or_path, false,
						&((EngineInternal*) internal)->espec,
						&((EngineInternal*) internal)->engine,
						&((EngineInternal*) internal)->dll);
		if (!result) {
			string np = __str(ENGINEDIR) "/" + string(name_or_path) + MODSUFFIX;
			result = do_load_file(np.c_str(), false,
							&((EngineInternal*) internal)->espec,
							&((EngineInternal*) internal)->engine,
							&((EngineInternal*) internal)->dll);
		}
	} else {
		result = do_load_dir(__str(ENGINEDIR), true,
					&((EngineInternal*) internal)->espec,
					&((EngineInternal*) internal)->engine,
					&((EngineInternal*) internal)->dll);
		if (!result)
			result = do_load_dir(__str(ENGINEDIR), false,
					&((EngineInternal*) internal)->espec,
					&((EngineInternal*) internal)->engine,
					&((EngineInternal*) internal)->dll);
	}

	if (!result)
		delete ((EngineInternal*) internal);
	return result;
}

bool Engine::initialize() {
	return initialize(NULL);
}

Value Engine::newGlobal(vector<string> path, vector<string> whitelist) {
	EngineInternal* ei = ((EngineInternal*) internal);
	EngineValue* glbl = ei->espec->newg(ei->engine);
	if (!glbl) throw bad_alloc();
	Value global = Value(glbl);

	// Add require function
	if (path.size() > 0) {
		RequireInternal* ri = new RequireInternal(path, whitelist);
		Value require = global.newFunction(require_js);
		require.setPrivate("natus_require", ri, RequireInternal::free);

		if (whitelist.size() == 0) {
			vector<Value> pathv;
			for (vector<string>::iterator i=path.begin(); i < path.end() ; i++)
				pathv.push_back(global.newString(*i));

			require.set("paths", global.newArray(pathv));
		}
		global.set("require", require, Value::Constant);
	}

	// Add in natus stuff
	global.set("natus", global.newObject(), Value::Constant);
	global.set("natus.engine", global.newObject(), Value::Constant);
	global.set("natus.engine.name", global.newString(this->getName()), Value::Constant);
	return global;
}

Value Engine::newGlobal(vector<string> path) {
	return newGlobal(path, vector<string>());
}

Value Engine::newGlobal() {
	return newGlobal(vector<string>(), vector<string>());
}

string Engine::getName() {
	EngineInternal* ei = (EngineInternal*) internal;
	return ei->espec->name;
}

Value::Value() {
	internal = NULL;
}

Value::Value(EngineValue* value) {
	internal = value;
	internal->incRef();
}

Value::Value(const Value& value) {
	internal = value.internal;
	internal->incRef();
}

Value::~Value() {
	if (internal) internal->decRef();
}

Value& Value::operator=(const Value& value) {
	if (internal) internal->decRef();
	internal = value.internal;
	internal->incRef();
	return *this;
}

Value Value::operator[](long index) {
	return get(index);
}

Value Value::operator[](string name) {
	return get(name);
}

Value Value::newBool(bool b) const {
	return internal->newBool(b);
}

Value Value::newNumber(double n) const {
	return internal->newNumber(n);
}

Value Value::newString(string string) const {
	return internal->newString(string);
}

Value Value::newArray(vector<Value> array) const {
	return internal->newArray(array);
}

Value Value::newFunction(NativeFunction func) const {
	return internal->newFunction(func);
}

Value Value::newObject(Class* cls) const {
	return internal->newObject(cls);
}

Value Value::newNull() const {
	return internal->newNull();
}

Value Value::newUndefined() const {
	return internal->newUndefined();
}

Value Value::getGlobal() const {
	return internal->getGlobal();
}

void Value::getContext(void **context, void **value) const {
	internal->getContext(context, value);
}

bool Value::isGlobal() const {
	return internal->isGlobal();
}

bool Value::isException() const {
	return internal->isException();
}

bool Value::isArray() const {
	return internal->isArray();
}

bool Value::isBool() const {
	return internal->isBool();
}

bool Value::isFunction() const {
	return internal->isFunction();
}

bool Value::isNull() const {
	return internal->isNull();
}

bool Value::isNumber() const {
	return internal->isNumber();
}

bool Value::isObject() const {
	return internal->isObject();
}

bool Value::isString() const {
	return internal->isString();
}

bool Value::isUndefined() const {
	return internal->isUndefined();
}

bool Value::toBool() const {
	if (isUndefined()) return false;
	return internal->toBool();
}

double Value::toDouble() const {
	if (isUndefined()) return 0;
	return internal->toDouble();
}

Value Value::toException() {
	return internal->toException(*this);
}

int Value::toInt() const {
	if (isUndefined()) return 0;
	return (int) toDouble();
}

long Value::toLong() const {
	if (isUndefined()) return 0;
	return (long) toDouble();
}

string Value::toString() const {
	return internal->toString();
}

vector<string> Value::toStringVector() const {
	vector<string> strv = vector<string>();
	long len = length();
	for (long i=0 ; i < len ; i++)
		strv.push_back(get(i).toString());
	return strv;
}

bool Value::del(string name) {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	return internal->del(name);
}

bool Value::del(long idx) {
	if (!isObject() && !isArray())
		return false;
	return internal->del(idx);
}

Value Value::get(string name) const {
	if (!isArray() && !isFunction() && !isObject())
		return newUndefined();

	if (name.find_first_of('.') == string::npos)
		return internal->get(name);

	return get(name.substr(0, name.find_first_of('.'))).get(name.substr(name.find_first_of('.')+1));
}

Value Value::get(long idx) const {
	if (!isObject() && !isArray())
		return newUndefined();
	return internal->get(idx);
}

bool Value::has(string name) const {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	Value v = get(name);
	return !v.isUndefined();
}

bool Value::has(long idx) const {
	if (!isObject() && !isArray())
		return false;
	Value v = get(idx);
	return !v.isUndefined();
}

bool Value::set(string name, Value value, Value::PropAttrs attrs) {
	if (!isArray() && !isFunction() && !isObject())
		return false;

	if (name.find_first_of('.') == string::npos)
		return internal->set(name, value, attrs);

	return get(name.substr(0, name.find_first_of('.'))).set(name.substr(name.find_first_of('.')+1), value, attrs);
}

bool Value::set(long idx, Value value) {
	if (!isArray() && !isFunction() && !isObject())
		return false;
	return internal->set(idx, value);
}

set<string> Value::enumerate() const {
	if (!isArray() && !isFunction() && !isObject())
		return std::set<string>();
	return internal->enumerate();
}

long Value::length() const {
	return get("length").toLong();
}

long Value::push(Value value) {
	vector<Value> args = vector<Value>();
	args.push_back(value);
	return call("push", args).toLong();
}

Value Value::pop() {
	return call("pop");
}

bool Value::setPrivate(string key, void *priv, FreeFunction free) {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return false;
	if (!(pm = internal->getPrivateMap())) return false;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end()) {
		if (it->second.second)
			it->second.second(it->second.first);
		pm->erase(it);
	}
	PrivateItem pi(priv, free);
	pm->insert(pair<string, PrivateItem>(key, pi));
	return true;
}

bool Value::setPrivate(string key, void *priv) {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return false;
	if (!(pm = internal->getPrivateMap())) return false;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end())
		return setPrivate(key, priv, it->second.second);
	return setPrivate(key, priv, NULL);
}

void* Value::getPrivate(string key) const {
	PrivateMap* pm;
	if (!internal->supportsPrivate()) return NULL;
	if (!(pm = internal->getPrivateMap())) return NULL;
	PrivateMap::iterator it = pm->find(key);
	if (it != pm->end())
		return it->second.first;
	return NULL;
}

Value Value::evaluate(string jscript, string filename, unsigned int lineno, bool shift) {
	if (shift && !isObject() && !isFunction()) return newUndefined();

	if (jscript.substr(0, 2) == "#!" && jscript.find_first_of('\n') != string::npos)
		jscript = jscript.substr(jscript.find_first_of('\n')+1);

	Value gl = getGlobal();
	RequireInternal* ri = (RequireInternal*) gl.get("require").getPrivate("natus_require");
	if (ri && !filename.empty()) ri->push(filename);

	Value v = internal->evaluate(jscript, filename, lineno, shift);

	if (ri && !filename.empty()) ri->pop();
	return v;
}

Value Value::call(Value func) {
	vector<Value> args = vector<Value>();
	return call(func, args);
}

Value Value::call(string func) {
	vector<Value> args = vector<Value>();
	return call(func, args);
}

Value Value::call(Value func, vector<Value> args) {
	if (!func.isObject() && !func.isFunction())
		return newUndefined();
	return internal->call(func, args);
}

Value Value::call(string func, vector<Value> args) {
	Value v = get(func);
	return call(v, args);
}

Value Value::callNew() {
	vector<Value> args = vector<Value>();
	return callNew(args);
}

Value Value::callNew(string func) {
	vector<Value> args = vector<Value>();
	return callNew(func, args);
}

Value Value::callNew(vector<Value> args) {
	if (!isObject() && !isFunction())
		return newUndefined();
	return internal->callNew(args);
}

Value Value::callNew(string func, vector<Value> args) {
	return get(func).callNew(args);
}

Value Value::require(string name, string reldir, vector<string> path) {
	vector<string> relpath;
	if (reldir.size() > 0)
		relpath.push_back(reldir);

	// Use the relative path
	if (name.substr(0, 2) == "./" || name.substr(0, 3) == "../")
		path = relpath;

	Value module = newUndefined();
	for (unsigned int i=0 ; i < path.size() ; i++) {
		string abspath = path[i] + "/" + name;
		abspath = abspath.substr(0, abspath.find_last_not_of('/')+1); // rstrip '/'

		// .so/.dll
		module = require_file(this, abspath+MODSUFFIX, name, false);

		// .js
		if (module.isUndefined()) {
			module = require_file(this, abspath+SCRSUFFIX, name, false);

			// /__init__.js
			if (module.isUndefined())
				module = require_file(this, abspath+"/"+PKGSUFFIX, name, false);
		}
		if (!module.isUndefined()) break;
	}

	if (!module.isUndefined())
		return module;

	// Throw not found exception
	return newString(name + NFSUFFIX).toException();
}

Value   Value::fromJSON(string json) {
	vector<Value> args;
	args.push_back(newString(json));
	Value obj = getGlobal().get("JSON");
	return obj.call("parse", args);
}

string  Value::toJSON() {
	vector<Value> args;
	args.push_back(*this);
	Value obj = getGlobal().get("JSON");
	return obj.call("stringify", args).toString();
}

EngineValue::EngineValue(EngineValue* glb, bool exception) {
	this->glb = glb;
	refCnt = 0;
	exc = exception;
}

void EngineValue::incRef() {
	refCnt++;
}

void EngineValue::decRef() {
	refCnt = refCnt > 0 ? refCnt-1 : 0;
	if (refCnt == 0)
		delete this;
}

Value EngineValue::toException(Value& value) {
	borrowInternal<EngineValue>(value)->exc = true;
	return value;
}

bool EngineValue::isException() {
	return exc;
}

Value EngineValue::getGlobal() {
	return Value(glb);
}

}
