#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "natus_module.h"

#define  _str(s) # s
#define __str(s) _str(s)

#define LIBDIR "/usr/lib64"
#define PROJECT "natus"
#define ENGINESDIR "engines"
#define MODSUFFIX ".so"

static void *malloc0(size_t size) {
	void *tmp = malloc(size);
	if (!tmp)
		return NULL;
	memset(tmp, 0, size);
	return tmp;
}

static ntValue *do_load_file(const char *filename, bool reqsym) {
	void *dll = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
	if (!dll)
		return NULL;

	struct _ntModule *module = dlsym(dll, __str(NT_MODULE_));
	if (!module || !module->init) {
		dlclose(dll);
		return NULL;
	}

	if (module->symb && reqsym) {
		void *tmp = dlopen(NULL, 0);
		if (!tmp || !dlsym(tmp, module->symb)) {
			if (tmp)
				dlclose(tmp);
			dlclose(dll);
			return NULL;
		}
		dlclose(tmp);
	}

	ntValue *self = NULL;
	if (!(self = module->init()) || self->vers != NT_MODULE_VERSION) {
		module->free(self);
		dlclose(dll);
		self = NULL;
	} else
		self->priv = dll;

	return self;
}

static ntValue *do_load_dir(const char *dirname, bool reqsym) {
	ntValue *self = NULL;
	DIR *dir = opendir(dirname);
	if (!dir)
		return NULL;

	struct dirent *ent = NULL;
	while ((ent = readdir(dir))) {
		if (strstr(ent->d_name, MODSUFFIX) != ent->d_name + strlen(ent->d_name)
				- strlen(MODSUFFIX))
			continue;
		char *tmp = malloc0(strlen(dirname) + strlen(ent->d_name) + 2);
		if (!tmp)
			break;
		sprintf(tmp, "%s/%s", dirname, ent->d_name);
		self = do_load_file(tmp, reqsym);
		free(tmp);
		if (self)
			break;
	}

	closedir(dir);
	return self;
}

ntValue *nt_global_init(const char *engine) {
	ntValue *self = NULL;

	if (engine) {
		char *tmpl = LIBDIR "/" PROJECT "/" ENGINESDIR "/%s" MODSUFFIX;
		char *tmp = malloc0(strlen(tmpl) + strlen(engine));
		if (!tmp)
			return NULL;

		sprintf(tmp, tmpl, engine);
		self = do_load_file(tmp, false);
		free(tmp);
		return self;
	}

	self = do_load_dir(ENGINESDIR, true);
	if (!self) {
		self = do_load_dir(LIBDIR "/" PROJECT "/" ENGINESDIR, true);
		if (!self) {
			self = do_load_dir(ENGINESDIR, false);
			if (!self)
				self = do_load_dir(LIBDIR "/" PROJECT "/" ENGINESDIR, false);
		}
	}

	return self;
}

void nt_global_free(ntValue *self) {
	assert(self);
	if (self->glbl)
		return; // Don't free non-globals

	struct _ntModule *module = dlsym(self->priv, __str(NT_JSENGINE_MODULE_));
	if (module && module->free)
		module->free(self);
	dlclose(self->priv);
	free(self);
}

const ntValue *nt_evaluate(ntValue *self, const char *jscript, const char *filename, int lineno) {
	assert(self);
	return self->evaluate(self, jscript, filename, lineno);
}

bool nt_is_global(ntValue *self) {
	assert(self);
	return (self->glbl == NULL);
}

ntType nt_type(const ntValue *self) {
	assert(self);
	return self->type(self);
}

bool nt_as_bool(const ntValue *self) {
	assert(self);
	return self->as_bool(self);
}

double nt_as_double(const ntValue *self) {
	assert(self);
	return self->as_double(self);
}

int nt_as_int(const ntValue *self) {
	assert(self);
	return self->as_int(self);
}

const char *nt_as_string(const ntValue *self) {
	assert(self);
	return self->as_string(self);
}

bool nt_property_del(ntValue *self, const char *key) {
	assert(self);
	return self->property_del(self, key);
}

const ntValue *nt_property_get(ntValue *self, const char *key) {
	assert(self);
	return self->property_get(self, key);
}

bool nt_property_set(ntValue *self, const char *key, ...) {
	assert(self);
	va_list args;
	va_start(args, key);
	bool retval = self->property_set(self, key, args);
	va_end(args);
	return retval;
}

void *nt_private_get(ntValue *self) {
	assert(self);
	return self->private_get(self);
}

bool nt_private_set(ntValue *self, void *priv) {
	assert(self);
	return self->private_set(self, priv);
}

bool nt_array_del(ntValue *self, int index) {
	assert(self);
	return self->array_del(self, index);
}

const ntValue *nt_array_get(ntValue *self, int index) {
	assert(self);
	return self->array_get(self, index);
}

bool nt_array_set(ntValue *self, int index, ...) {
	assert(self);
	va_list args;
	va_start(args, index);
	bool retval = self->array_set(self, index, args);
	va_end(args);
	return retval;
}

int nt_array_len(ntValue *self) {
	assert(self);
	return self->array_len(self);
}

bool nt_function_oom(ntValue **ret) {
	assert(ret);
	*ret = NULL;
	return false;
}

void nt_function_return(ntValue **ret, ...) {
	assert(ret);
	va_list args;
	va_start(args, ret);
	(*ret)->function_return(*ret, args);
	va_end(args);
}

bool nt_import(ntValue **ret, ntValue *glb, ntValue *ths, ntValue *fnc, ntValue **arg, void *data) {
	if (!arg || !*arg || nt_type(*arg) != NT_TYPE_STRING) {
		nt_function_return(ret, ARG_STRING("Invalid number/type of arguments!"));
		return false;
	}

	char *module = strdup(nt_as_string(*arg));
	printf("%s\n", module);
	return true;
}
