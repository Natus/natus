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
#include <cassert>

#include <fstream>
#include <stack>
#include <vector>

#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>

#define SCRSUFFIX  ".js"
#define PKGSUFFIX  "__init__" SCRSUFFIX
#define URIPREFIX  "file://"
#define NFSUFFIX   " not found!"

#define PRV_MODULE_LOADER        "natus.ModuleLoader"
#define PRV_MODULE_LOADER_CONFIG "natus.ModuleLoader.config"
#define CFG_PATH                 "natus.path"
#define CFG_WHITELIST            "natus.whitelist"
#define CFG_ORIGINS_WHITELIST    "natus.origins.whitelist"
#define CFG_ORIGINS_BLACKLIST    "natus.origins.blacklist"

#define MLI(i) static_cast<ModuleLoaderInternal*>(i)

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
namespace natus {

struct FunctionHook {
	FreeFunction    free;
	void*           misc;

	~FunctionHook() {
		if (free && misc)
			free(misc);
	}
};

struct RequireHook : public FunctionHook {
	bool            post;
	RequireFunction func;
	RequireHook(FreeFunction free, void* misc, RequireFunction func, bool post) {
		this->free = free;
		this->misc = misc;
		this->func = func;
		this->post = post;
	}
};

struct OriginHook : public FunctionHook {
	OriginMatcher func;
	OriginHook(FreeFunction free, void* misc, OriginMatcher func) {
		this->free = free;
		this->misc = misc;
		this->func = func;
	}
};

static Value require_file(Value base, string abspath) {
	// Stat our file
	struct stat st;
	if (stat(abspath.c_str(), &st))
		return base.newUndefined().toException();

	Value require = base.getGlobal().get("require");

	if (!require.get("paths").isUndefined())
		base.get("module").set("uri", base.newString(URIPREFIX+abspath), Value::Constant);

	// If we have a binary module -- .so/.dll
	if (abspath.substr(abspath.length() - string(MODSUFFIX).length(), string(MODSUFFIX).length()) == MODSUFFIX) {
		void *dll = dlopen(abspath.c_str(), RTLD_NOW | RTLD_GLOBAL);
		if (!dll)
			return base.newString(dlerror()).toException();

		// Add our current directory to the require stack
		Value reqStack = require.getPrivateValue("natus.reqStack");
		if (reqStack.isArray()) {
			char *tmp = strdup(abspath.c_str());
			if (tmp) {
				vector<Value> pushargs;
				pushargs.push_back(reqStack.newString(dirname(tmp)));
				std::free(tmp);
				reqStack.call("push", pushargs);
			}
		}

		bool (*load)(Value&) = (bool (*)(Value&)) dlsym(dll, "natus_require");
		if (!load || !load(base)) {
			if (reqStack.isArray())
				reqStack.call("pop");
			dlclose(dll);
			return base.newString("Module initialization failed!").toException();
		}
		if (reqStack.isArray())
			reqStack.call("pop");
		base.get("exports").setPrivate("natus.Module", dll, (FreeFunction) dlclose);

		return base;
	}

	// If we have a javascript text module
	ifstream file(abspath.c_str());
	if (!file.is_open()) return base.newUndefined().toException();
	char *jscript = new char[st.st_size + 1];
	file.read(jscript, st.st_size);
	if (!file.good() && !file.eof()) {
		file.close();
		delete jscript;
		return base.newString("Error reading file!").toException();
	}
	file.close();

	Value res = base.evaluate(jscript, abspath, 0, true);
	delete[] jscript;
	if (res.isException())
		return res;
	return base;
}

static Value internal_require(Value& module, string& name, string& reldir, vector<string>& path) {
	vector<string> relpath;
	if (reldir.size() > 0)
		relpath.push_back(reldir);

	// Use the relative path
	if (name.substr(0, 2) == "./" || name.substr(0, 3) == "../")
		path = relpath;

	Value res;
	for (unsigned int i=0 ; i < path.size() ; i++) {
		string abspath = path[i] + "/" + name;
		abspath = abspath.substr(0, abspath.find_last_not_of('/')+1); // rstrip '/'

		// .so/.dll
		res = require_file(module, abspath+MODSUFFIX);

		// .js
		if (res.isUndefined()) {
			res = require_file(module, abspath+SCRSUFFIX);

			// /__init__.js
			if (res.isUndefined())
				res = require_file(module, abspath+"/"+PKGSUFFIX);
		}
		if (!res.isUndefined()) break;
	}

	if (!res.isUndefined())
		return res;

	// Throw not found exception
	return module.newString(name + NFSUFFIX).toException();
}

static Value require_js(Value& ths, Value& fnc, vector<Value>& arg) {
	// Get the required name argument
	Value exc = checkArguments(ths, arg, "s");
	if (exc.isException()) return exc;
	string module = arg[0].toString();

	// Build our path and whitelist
	Value config    = ModuleLoader::getConfig(ths);
	Value path      = config.get(CFG_PATH);
	Value whitelist = config.get(CFG_WHITELIST);

	// Security check
	if (whitelist.isArray()) {
		bool cleared = false;
		for (unsigned int i=0 ; !cleared && i < whitelist.get("length").toLong() ; i++)
			cleared = whitelist.get(i).toString() == module;
		if (!cleared) return throwException(ths, "SecurityError", "Permission denied!");
	}

	Value reqStack = fnc.getGlobal().getPrivateValue("natus.reqStack");
	Value top = reqStack.newUndefined();
	if (reqStack.get("length").toLong() > 0)
		top = reqStack.get(reqStack.get("length").toLong()-1);

	ModuleLoader* ml = ModuleLoader::getModuleLoader(ths);
	if (!ml) return throwException(ths, "ImportError", "Unable to find module loader!");

	return ml->require(module, top.toString(), path.toStringVector());
}

ModuleLoader* ModuleLoader::getModuleLoader(const Value& ctx) {
	return static_cast<ModuleLoader*>(ctx.getGlobal().getPrivate(PRV_MODULE_LOADER));
}

Value ModuleLoader::getConfig(const Value& ctx) {
	return ctx.getGlobal().getPrivateValue(PRV_MODULE_LOADER_CONFIG);
}

Value ModuleLoader::checkOrigin(const Value& ctx, string uri) {
	ModuleLoader* ml = getModuleLoader(ctx);
	if (!ml)
		return throwException(ctx, "RuntimeError", "Unable to find module loader!");
	if (!ml->originPermitted(uri))
		return throwException(ctx, "PermissionDenied", "Not permitted to access: " + uri);
	return ctx.newUndefined();
}

struct ModuleLoaderInternal {
	Value               glb;
	vector<RequireHook> rh;
	vector<OriginHook>  oh;
};

ModuleLoader::ModuleLoader(Value& ctx) {
	internal = new ModuleLoaderInternal();
	MLI(internal)->glb = ctx.getGlobal();
}

ModuleLoader::~ModuleLoader() {
	MLI(internal)->glb.setPrivate(PRV_MODULE_LOADER, NULL);
	MLI(internal)->glb.setPrivate(PRV_MODULE_LOADER_CONFIG, NULL);
	MLI(internal)->glb.del("require");
	delete MLI(internal);
}

Value ModuleLoader::initialize(string cfg) {
	// Make sure we haven't already initialized a module loader
	if (MLI(internal)->glb.getPrivate(PRV_MODULE_LOADER) != NULL)
		return throwException(MLI(internal)->glb, "RuntimeError", "ModuleLoader already initialized!");
	MLI(internal)->glb.setPrivate(PRV_MODULE_LOADER, this);

	// Parse the config
	Value config = MLI(internal)->glb.fromJSON(cfg);
	if (config.isException()) return config;
	MLI(internal)->glb.setPrivate(PRV_MODULE_LOADER_CONFIG, config);

	// Add require function
	Value path = config.get(CFG_PATH);
	if (path.isArray() && path.get("length").toLong() > 0) {
		Value require = MLI(internal)->glb.newFunction(require_js);
		require.setPrivate("natus.reqStack", require.newArray());

		Value whitelist = config.get(CFG_WHITELIST);
		if (!whitelist.isArray())
			require.set("paths", path);
		MLI(internal)->glb.set("require", require, Value::Constant);
	}

	return MLI(internal)->glb.newBool(true);
}


Value ModuleLoader::require(string name, string reldir, vector<string> path) const {
	// Setup our import environment
	Value res;
	Value base    = MLI(internal)->glb.newObject();
	Value exports = MLI(internal)->glb.newObject();
	Value module  = MLI(internal)->glb.newObject();
	if (base.isUndefined() || exports.isUndefined() || module.isUndefined())      goto notfound;
	if (!base.set("exports", exports, Value::Constant))                           goto notfound;
	if (!base.set("module", module, Value::Constant))                             goto notfound;
	if (!module.set("id", MLI(internal)->glb.newString(name), Value::Constant))   goto notfound;
	if (!base.set("require", MLI(internal)->glb.get("require"), Value::Constant)) goto notfound;

	// Go through the pre hooks
	for (vector<RequireHook>::iterator hook=MLI(internal)->rh.begin() ; hook != MLI(internal)->rh.end() ; hook++) {
		if (hook->post || !hook->func) continue;
		res = hook->func(base, name, reldir, path, hook->misc);
		if (res.isException())
			return res;
		else if (!res.isUndefined())
			goto modfound;
	}

	// If none of the pre-hooks found the module, use the internal loader
	res = internal_require(base, name, reldir, path);
	if (res.isException())
		return res;
	else if (!res.isUndefined())
		goto modfound;

	// Module not found
	notfound:
		return MLI(internal)->glb.newString(name + NFSUFFIX).toException();

	// Module found
	modfound:
		exports.set("__module__", module, Value::Constant);
		for (vector<RequireHook>::iterator hook=MLI(internal)->rh.begin() ; hook != MLI(internal)->rh.end() ; hook++) {
			if (!hook->post || !hook->func) continue;
			Value exc = hook->func(exports, name, reldir, path, hook->misc);
			if (exc.isException())
				return exc;
		}
		return exports;
}

int   ModuleLoader::addRequireHook(bool post, RequireFunction func, void* misc, FreeFunction free) {
	MLI(internal)->rh.push_back(RequireHook(free, misc, func, post));
	return MLI(internal)->rh.size()-1;
}

void  ModuleLoader::delRequireHook(int id) {
	vector<RequireHook>* hooks = &MLI(internal)->rh;
	if (!hooks || id >= (int) hooks->size()) return;
	if (hooks->at(id).free && hooks->at(id).misc)
		hooks->at(id).free(hooks->at(id).misc);
	hooks->at(id).func = NULL;
	hooks->at(id).free = NULL;
	hooks->at(id).misc = NULL;
}

int   ModuleLoader::addOriginMatcher(OriginMatcher func, void* misc, FreeFunction free) {
	MLI(internal)->oh.push_back(OriginHook(free, misc, func));
	return MLI(internal)->oh.size()-1;
}

void  ModuleLoader::delOriginMatcher(int id) {
	vector<OriginHook>* hooks = &MLI(internal)->oh;
	if (!hooks || id >= (int) hooks->size()) return;
	if (hooks->at(id).free && hooks->at(id).misc)
		hooks->at(id).free(hooks->at(id).misc);
	hooks->at(id).func = NULL;
	hooks->at(id).free = NULL;
	hooks->at(id).misc = NULL;
}

bool  ModuleLoader::originPermitted(string uri) const {
	Value whitelist = getConfig(MLI(internal)->glb).get(CFG_ORIGINS_WHITELIST);
	Value blacklist = getConfig(MLI(internal)->glb).get(CFG_ORIGINS_BLACKLIST);
	if (!whitelist.isArray()) return true;

	bool permitted = false;
	for (vector<OriginHook>::iterator it=MLI(internal)->oh.begin() ; !permitted && it != MLI(internal)->oh.end() ; it++) {
		long len = whitelist.get("length").toLong();
		for (long i=0 ; !permitted && i < len ; i++) {
			if (!it->func) continue;
			permitted = it->func(whitelist.get(i).toString().c_str(), uri.c_str());
		}
	}

	if (permitted && blacklist.isArray()) {
		for (vector<OriginHook>::iterator it=MLI(internal)->oh.begin() ; permitted && it != MLI(internal)->oh.end() ; it++) {
			long len = blacklist.get("length").toLong();
			for (long i=0 ; permitted && i < len ; i++) {
				if (!it->func) continue;
				permitted = !it->func(blacklist.get(i).toString().c_str(), uri.c_str());
			}
		}
	}

	return permitted;
}

}
