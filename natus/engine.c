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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "backend.h"
#include "engine.h"

#define  _str(s) # s
#define __str(s) _str(s)

static inline bool do_load_file(const char *filename, bool reqsym, ntEngineSpec** enginespec, void** engine, void** dll) {
	void *dll_ = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
	if (!dll_) {
		//printf("%s -- %s\n", filename.c_str(), dlerror());
		return false;
	}

	ntEngineSpec* module = (ntEngineSpec*) dlsym(dll_, __str(NATUS_ENGINE_));
	if (!module || module->version != NATUS_ENGINE_VERSION || !module->engine.init) {
		dlclose(dll_);
		return false;
	}

	if (module->symbol && reqsym) {
		void *tmp = dlopen(NULL, 0);
		if (!tmp || !dlsym(tmp, module->symbol)) {
			if (tmp)
				dlclose(tmp);
			dlclose(dll_);
			return false;
		}
		dlclose(tmp);
	}

	void *eng = NULL;
	if (!(eng = module->engine.init())) {
		dlclose(dll_);
		return false;
	}

	*dll = dll_;
	*engine = eng;
	*enginespec = module;
	return true;
}

static inline bool do_load_dir(const char *dirname, bool reqsym, ntEngineSpec** enginespec, void** engine, void** dll) {
	bool res = false;
	DIR *dir = opendir(dirname);
	if (!dir)
		return NULL;

	struct dirent *ent = NULL;
	while ((ent = readdir(dir))) {
		size_t flen = strlen(ent->d_name);
		size_t slen = strlen(MODSUFFIX);

		if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name))     continue;
		if (flen < slen || strcmp(ent->d_name + flen - slen, MODSUFFIX)) continue;

		char *tmp = (char *) malloc(sizeof(char) * (flen + strlen(dirname) + 2));
		if (!tmp) continue;

		strcpy(tmp, dirname);
		strcat(tmp, "/");
		strcat(tmp, ent->d_name);

		if ((res = do_load_file(tmp, reqsym, enginespec, engine, dll))) {
			free(tmp);
			break;
		}
		free(tmp);
	}

	closedir(dir);
	return res;
}

ntEngine *nt_engine_init(const char *name_or_path) {
	ntEngine *self = malloc(sizeof(ntEngine));
	if (!self) return NULL;
	self->ref = 1;

	bool result = false;
	if (name_or_path) {
		result = do_load_file(name_or_path, false, &self->espec, &self->engine, &self->dll);
		if (!result) {
			char *tmp = malloc(strlen(name_or_path) + strlen(__str(ENGINEDIR) MODSUFFIX) + 2);
			if (tmp) {
				sprintf(tmp, "%s/%s%s", __str(ENGINEDIR), name_or_path, MODSUFFIX);
				result = do_load_file(tmp, false, &self->espec, &self->engine, &self->dll);
				free(tmp);
			}
		}
	} else {
		result = do_load_dir(__str(ENGINEDIR), true, &self->espec, &self->engine, &self->dll);
		if (!result)
			result = do_load_dir(__str(ENGINEDIR), false, &self->espec, &self->engine, &self->dll);
	}

	return result ? self : nt_engine_decref(self);
}

ntEngine *nt_engine_incref(ntEngine *engine) {
	if (!engine) return NULL;
	engine->ref++;
	return engine;
}

ntEngine *nt_engine_decref(ntEngine *engine) {
	if (!engine) return NULL;
	engine->ref = engine->ref > 0 ? engine->ref - 1 : 0;
	if (engine->ref == 0) {
		if (engine->espec && engine->espec->engine.free && engine->engine)
			engine->espec->engine.free(engine->engine);
		if (engine->dll)
			dlclose(engine->dll);
		free(engine);
		return NULL;
	}
	return engine;
}

const char *nt_engine_get_name(ntEngine *engine) {
	return engine->espec->name;
}

ntValue *nt_engine_new_global(ntEngine *engine, ntValue *global) {
	if (!engine || !engine->espec || !engine->espec->engine.newg || !engine->engine || (global && global->eng != engine))
		return NULL;
	ntValue *glb = engine->espec->engine.newg(engine->engine, global);
	if (glb) {
		glb->eng = nt_engine_incref(engine);
		glb->ref = 1;
		glb->typ = ntValueTypeObject;
		nt_value_set_private_name(glb, NATUS_PRIV_GLOBAL, glb, NULL);
	}
	return glb;
}
