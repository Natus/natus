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

#include <stack>

#include <cstring>
#include <cstdlib>

#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
#include "engine.h"

#define  _str(s) # s
#define __str(s) _str(s)

namespace natus {

struct EngineInternal {
	EngineSpec*   espec;
	void*         engine;
	void*         dll;
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

	if (!result) {
		delete ((EngineInternal*) internal);
		internal = NULL;
	}
	return result;
}

bool Engine::initialize() {
	return initialize(NULL);
}

Value Engine::newGlobal() {
	EngineInternal* ei = ((EngineInternal*) internal);
	EngineValue* glbl = ei->espec->newg(ei->engine);
	if (!glbl) throw bad_alloc();

	// Add in natus stuff
	Value global = Value(glbl);
	global.set("natus", global.newObject(), Value::Constant);
	global.set("natus.engine", global.newObject(), Value::Constant);
	global.set("natus.engine.name", global.newString(this->getName()), Value::Constant);
	return global;
}

string Engine::getName() {
	EngineInternal* ei = (EngineInternal*) internal;
	return ei->espec->name;
}

}
