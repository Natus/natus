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

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>

#define SCRSUFFIX  ".js"
#define PKGSUFFIX  "/__init__" SCRSUFFIX
#define URIPREFIX  "file://"
#define NFSUFFIX   " not found!"

#define NATUS_REQUIRE         "natus::Require"
#define NATUS_REQUIRE_STACK   "natus::Require::Stack"
#define CFG_PATH              "natus.require.path"
#define CFG_WHITELIST         "natus.require.whitelist"
#define CFG_ORIGINS_WHITELIST "natus.origins.whitelist"
#define CFG_ORIGINS_BLACKLIST "natus.origins.blacklist"

#include "value.h"
#include "misc.h"
#include "private.h"
#include "require.h"
#include "vsprintf.h"

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "backend.h"

#define  _str(s) # s
#define __str(s) _str(s)

typedef struct {
	ntValue   *config;
	ntPrivate *hooks;
	ntPrivate *matchers;
	ntPrivate *dll;
	ntPrivate *modules;
} ntRequire;

static void _nt_require_free(ntRequire *req) {
	if (!req) return;
	nt_value_decref(req->config);
	nt_private_free(req->hooks);
	nt_private_free(req->matchers);
	nt_private_free(req->dll);
	nt_private_free(req->modules);
	free(req);
}


typedef struct {
	void           *misc;
	ntFreeFunction  free;
} reqPayload;

typedef struct {
	reqPayload      head;
	ntRequireOriginMatcher func;
} reqOriginMatcher;

typedef struct {
	reqPayload        head;
	ntRequireHook func;
} reqHook;

static void _free_pl(reqPayload *pl) {
	if (!pl) return;
	if (pl->misc && pl->free)
		pl->free(pl->misc);
	free(pl);
}

typedef struct {
	      char *pat;
	const char *uri;
	bool        match;
} loopData;

static void _foreach_match(const char *name, reqOriginMatcher *om, loopData *ld) {
	if (!om->func || ld->match) return;
	ld->match = om->func(ld->pat, ld->uri);
}

static char *check_path(struct stat *st, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *fn = _vsprintf(fmt, ap);
	va_end(ap);
	if (!fn) return NULL;

	char *rp = realpath(fn, NULL);
	free(fn);
	if (!rp) return NULL;

	if (stat(rp, st) == 0) return rp;
	free(rp);
	return NULL;
}

static ntValue* internal_require(ntValue *module, ntRequireHookStep step, char *name, void *misc) {
	ntRequire *req = nt_value_private_get(module, NATUS_REQUIRE);
	if (!req) return NULL;

	ntValue *path = NULL;

	// If the path is a relative path, use the top of the evaluation stack
	if (name && name[0] == '.') {
		ntValue *global = nt_value_get_global(module);
		ntValue *stack  = nt_value_private_get_value(global, NATUS_REQUIRE_STACK);
		nt_value_decref(global);

		long len = nt_value_as_long(nt_value_get_utf8(stack, "length"));
		if (len <= 0) {
			nt_value_decref(stack);
			return NULL;
		}

		ntValue *prfx = nt_value_get_index(stack, len  - 1);
		nt_value_decref(stack);
		if (!prfx) return NULL;

		path = nt_value_new_array_builder(module, prfx);
		nt_value_decref(prfx);

	// Otherwise use the normal path
	} else {
		ntValue *config = nt_require_get_config(module);
		if (!config) return NULL;

		path = nt_value_get_recursive_utf8(config, CFG_PATH);
		nt_value_decref(config);
	}

	struct stat st;
	long i, len = nt_value_as_long(nt_value_get_utf8(path, "length"));
	for (i=0 ; i < len ; i++) {
		ntValue *retval = NULL;
		char *prefix = nt_value_as_string_utf8(nt_value_get_index(path, i), NULL);

		// Check for native modules
		char *file = check_path(&st, "%s/%s%s", prefix, name, MODSUFFIX);
		if (file) {
			if (step == ntRequireHookStepResolve) goto out;

			void *dll = dlopen(file, RTLD_NOW | RTLD_GLOBAL);
			if (!dll) {
				retval = nt_throw_exception(module, "ImportError", dlerror());
				goto out;
			}

			ntRequireModuleInit load = (ntRequireModuleInit) dlsym(dll, __str(NATUS_MODULE_INIT));
			if (!load || !load(module)) {
				dlclose(dll);
				retval = nt_throw_exception(module, "ImportError", "Module initialization failed!");
				goto out;
			}

			nt_private_push(req->dll, dll, (ntFreeFunction) dlclose);

			retval = nt_value_new_boolean(module, true);
			goto out;
		}

		// Check for javascript modules
		file = check_path(&st, "%s/%s.js", prefix, name);
		if (!file) file = check_path(&st, "%s/%s/__init__.js", prefix, name);
		if (file) {
			if (step == ntRequireHookStepResolve) goto out;

			// If we have a javascript text module
			FILE *modfile = fopen(file, "r");
			if (!modfile) return NULL;

			char *jscript = calloc(st.st_size + 1, sizeof(char));
			memset(jscript, 0, st.st_size + 1);
			if (fread(jscript, 1, st.st_size, modfile) != st.st_size) {
				fclose(modfile);
				free(jscript);
				retval = nt_throw_exception(module, "ImportError", "Error reading file!");
				goto out;
			}
			fclose(modfile);

			retval = nt_value_evaluate_utf8(module, jscript, file, 0);
			free(jscript);
			goto out;
		}

		free(prefix);
		continue;

		out:
			strncpy(name, "file://", PATH_MAX);
			strncat(name, file, PATH_MAX - strlen("file://"));
			nt_value_decref(path);
			free(prefix);
			free(file);
			return retval;
	}
	nt_value_decref(path);
	return nt_throw_exception(module, "ImportError", "Module '%s' not found!", name);
}

static ntValue *require_js(ntValue *require, ntValue *ths, ntValue *arg) {
	NT_CHECK_ARGUMENTS(arg, "s");

	// Get the required name argument
	char *modname = nt_value_as_string_utf8(nt_value_get_index(arg, 0), NULL);

	// Build our path and whitelist
	ntValue       *cfg = nt_require_get_config(require);
	ntValue      *path = nt_value_get_recursive_utf8(cfg, CFG_PATH);
	ntValue *whitelist = nt_value_get_recursive_utf8(cfg, CFG_WHITELIST);

	// Security check
	if (nt_value_is_array(whitelist)) {
		bool cleared = false;
		long i, len = nt_value_as_long(nt_value_get_utf8(whitelist, "length"));
		for (i=0 ; !cleared && i < len ; i++) {
			char *item = nt_value_as_string_utf8(nt_value_get_index(whitelist, i), NULL);
			cleared = !strcmp(modname, item);
			free(item);
		}
		if (!cleared) {
			nt_value_decref(cfg);
			nt_value_decref(path);
			nt_value_decref(whitelist);
			return nt_throw_exception(ths, "SecurityError", "Permission denied!");
		}
	}

	ntValue *mod = nt_require(require, modname);
	free(modname);
	nt_value_decref(cfg);
	nt_value_decref(path);
	nt_value_decref(whitelist);
	return mod;
}


bool              nt_require_init              (ntValue *ctx, const char *config) {
	// Parse the config
	if (!config) config = "{}";
	ntValue *cfg = nt_from_json_utf8(ctx, config, strlen(config));
	if (nt_value_is_exception(cfg)) {
		nt_value_decref(cfg);
		return false;
	}

	bool rslt = nt_require_init_value(ctx, cfg);
	nt_value_decref(cfg);
	return rslt;
}

bool              nt_require_init_value        (ntValue *ctx, ntValue *config) {
	ntValue *glb = NULL, *pth = NULL;
	ntRequire *req = NULL;

	// Get the global
	glb = nt_value_get_global(ctx);
	if (!glb) goto error;

	// Make sure we haven't already initialized a module loader
	// If not, initialize
	req = nt_value_private_get(glb, NATUS_REQUIRE);
	if (req) {
		nt_value_decref(glb);
		return true;
	}
	if (!(req = malloc(sizeof(ntRequire)))) goto error;
	memset(req, 0, sizeof(ntRequire));
	req->config   = nt_value_incref(config);
	req->hooks    = nt_private_init();
	req->matchers = nt_private_init();
	req->dll      = nt_private_init();
	req->modules  = nt_private_init();
	if (!req->hooks || !req->matchers || !req->dll || !req->modules)
		goto error;

	// Add require function
	pth = nt_value_get_recursive_utf8(req->config, CFG_PATH);
	if (pth && nt_value_is_array(pth) && nt_value_as_long(nt_value_get_utf8(pth, "length")) > 0) {
		// The stack is the one big exception to the layer boundary since it needs to be
		// updated by the nt_value_evalue() function.
		ntValue *stack = nt_value_new_array(glb, NULL);
		if (!nt_value_is_array(stack) || !nt_value_private_set_value(glb, NATUS_REQUIRE_STACK, stack)) {
			nt_value_decref(stack);
			goto error;
		}
		nt_value_decref(stack);

		// Setup the require function
		ntValue *require = nt_value_new_function(glb, require_js);
		if (!nt_value_is_function(require) || !nt_value_as_bool(nt_value_set_utf8(glb, "require", require, ntPropAttrConstant))) {
			nt_value_decref(require);
			goto error;
		}

		// Add the paths variable if we are not in a sandbox
		ntValue *whitelist = nt_value_get_utf8(req->config, CFG_WHITELIST);
		if (!nt_value_is_array(whitelist))
			nt_value_decref(nt_value_set_utf8(require, "paths", pth, ntPropAttrConstant));
		nt_value_decref(whitelist);

		nt_value_decref(require);
	}
	nt_value_decref(pth);

	// Finish initialization
	if (!nt_value_private_set(glb, NATUS_REQUIRE, req, (ntFreeFunction) _nt_require_free)
			|| !nt_require_hook_add(glb, "natus::InternalLoader", internal_require, NULL, NULL)) {
		nt_value_private_set(glb, NATUS_REQUIRE, NULL, NULL);
		nt_value_private_set(glb, NATUS_REQUIRE_STACK, NULL, NULL);
		nt_value_del_utf8(glb, "require");
		nt_value_decref(glb);
		return false;
	}
	nt_value_decref(glb);
	return true;

	error:
		_nt_require_free(req);
		nt_value_decref(glb);
		nt_value_decref(pth);
		return false;
}

void              nt_require_free              (ntValue *ctx) {
	ntValue *glb = nt_value_get_global(ctx);
	nt_value_private_set(glb, NATUS_REQUIRE, NULL, NULL);
	nt_value_private_set(glb, NATUS_REQUIRE_STACK, NULL, NULL);
	nt_value_decref(glb);
}

ntValue          *nt_require_get_config        (ntValue *ctx) {
	// Get the global and the ntRequire struct
	ntValue  *global = nt_value_get_global(ctx);
	ntRequire   *req = nt_value_private_get(global, NATUS_REQUIRE);
	if (!req) {
		nt_value_decref(global);
		return NULL;
	}

	return nt_value_incref(req->config);
}

#define _do_set(ctx, field, name, item, free) \
	ntValue *glb = nt_value_get_global(ctx); \
	if (!glb) goto error; \
	ntRequire *req = (ntRequire*) nt_value_private_get(glb, NATUS_REQUIRE); \
	if (!req) goto error; \
	return nt_private_set(req->field, name, item, ((ntFreeFunction) free)); \
	error: \
		if (item) free((reqPayload*) item); \
		nt_value_decref(glb); \
		return false;

bool              nt_require_hook_add          (ntValue *ctx, const char *name, ntRequireHook func, void *misc, ntFreeFunction free) {
	reqHook *hook = malloc(sizeof(reqHook));
	if (!hook) return false;
	hook->head.misc = misc;
	hook->head.free = free;
	hook->func = func;

	_do_set(ctx, hooks, name, hook, _free_pl);
}

bool              nt_require_hook_del          (ntValue *ctx, const char *name) {
	_do_set(ctx, hooks, name, NULL, _free_pl);
}

bool              nt_require_origin_matcher_add(ntValue *ctx, const char *name, ntRequireOriginMatcher func, void *misc, ntFreeFunction free) {
	reqOriginMatcher *match = malloc(sizeof(reqOriginMatcher));
	if (!match) return false;
	match->head.misc = misc;
	match->head.free = free;
	match->func = func;

	_do_set(ctx, matchers, name, match, _free_pl);
}

bool              nt_require_origin_matcher_del(ntValue *ctx, const char *name) {
	_do_set(ctx, matchers, name, NULL, _free_pl);
}

bool nt_require_origin_permitted(ntValue *ctx, const char *uri) {
	// Get the global
	ntValue *glb = nt_value_get_global(ctx);
	if (!glb) return true;

	// Get the require structure
	ntRequire *req = nt_value_private_get(glb, NATUS_REQUIRE);
	if (!req) {
		nt_value_decref(glb);
		return true;
	}

	// Get the whitelist and blacklist
	ntValue *whitelist = nt_value_get_recursive_utf8(req->config, CFG_ORIGINS_WHITELIST);
	ntValue *blacklist = nt_value_get_recursive_utf8(req->config, CFG_ORIGINS_BLACKLIST);
	if (!nt_value_is_array(whitelist)) {
		nt_value_decref(blacklist);
		nt_value_decref(whitelist);
		nt_value_decref(glb);
		return true;
	}

	// Check to see if the URI is on the approved whitelist
	int i;
	loopData ld = { .match = false, .uri = uri };
	long len = nt_value_as_long(nt_value_get_utf8(whitelist, "length"));
	for (i=0 ; !ld.match && i < len ; i++) {
		ld.pat = nt_value_as_string_utf8(nt_value_get_index(whitelist, i), NULL);
		if (!ld.pat) continue;
		nt_private_foreach(req->matchers, false, (ntPrivateForeach) _foreach_match, &ld);
		free(ld.pat);
	}
	nt_value_decref(whitelist);

	// If the URI matched the whitelist, check the blacklist
	if (ld.match && nt_value_is_array(blacklist)) {
		ld.match = false;
		len = nt_value_as_long(nt_value_get_utf8(blacklist, "length"));
		for (i=0 ; !ld.match && i < len ; i++) {
			ld.pat = nt_value_as_string_utf8(nt_value_get_index(blacklist, i), NULL);
			if (!ld.pat) continue;
			nt_private_foreach(req->matchers, false, (ntPrivateForeach) _foreach_match, &ld);
			free(ld.pat);
		}
		ld.match = !ld.match;
	}
	nt_value_decref(blacklist);

	return ld.match;
}

static ntValue *prep_module_base(ntValue *global, const char *name) {
	ntEngine *engine = nt_value_get_engine(global);
    ntValue    *base = nt_engine_new_global(engine, global);
	ntValue *require = nt_value_get_utf8(global, "require");
	ntValue *exports = nt_value_new_object(global, NULL);
	ntValue  *module = nt_value_new_object(global, NULL);
	ntValue *modname = nt_value_new_string_utf8(global, name);
	nt_engine_decref(engine);
	if (nt_value_is_exception(base)
			|| nt_value_is_exception(require)
			|| nt_value_is_exception(exports)
			|| nt_value_is_exception(module)
			|| nt_value_is_exception(modname))
		goto error;
	if (!nt_value_private_set(base, NATUS_REQUIRE,       nt_value_private_get(global, NATUS_REQUIRE),       NULL)) goto error;
	if (!nt_value_private_set(base, NATUS_REQUIRE_STACK, nt_value_private_get(global, NATUS_REQUIRE_STACK), NULL)) goto error;
	if (!nt_value_as_bool(nt_value_set_utf8(base,   "exports", exports, ntPropAttrConstant))) goto error;
	if (!nt_value_as_bool(nt_value_set_utf8(base,   "module",  module,  ntPropAttrConstant))) goto error;
	if (!nt_value_as_bool(nt_value_set_utf8(module, "id",      modname, ntPropAttrConstant))) goto error;
	if (!nt_value_as_bool(nt_value_set_utf8(base,   "require", require, ntPropAttrConstant))) goto error;
	nt_value_decref(require);
	nt_value_decref(exports);
	nt_value_decref(module);
	nt_value_decref(modname);
	return base;

	error:
		nt_value_decref(base);
		nt_value_decref(require);
		nt_value_decref(exports);
		nt_value_decref(module);
		nt_value_decref(modname);
		return NULL;
}

typedef struct {
	const char           *name;
	ntRequire            *req;
	ntRequireHookStep step;
	ntValue              *module;
	ntValue              *res;
} ntHookData;

static void do_hooks(const char *hookname, reqHook *hook, ntHookData *misc) {
	if (!misc || misc->res) return;
	char name[PATH_MAX];
	strcpy(name, misc->name);

	// Call the hook
	misc->res = hook->func(misc->module, misc->step, name, hook->head.misc);

	// If module is NULL, we're just checking for cache names
	if (misc->step == ntRequireHookStepResolve) {
		nt_value_decref(misc->res);

		// If the name was found in the cache, return it
		misc->res = nt_private_get(misc->req->modules, name);
		return;
	}

	// If a module was found, cache it according to the (possibly modified) name
	if (misc->step == ntRequireHookStepLoad && !nt_value_is_exception(misc->res) && !nt_value_is_undefined(misc->res)) {
		ntValue *whitelist = nt_value_get_utf8(misc->req->config, CFG_WHITELIST);
		if (!nt_value_is_array(whitelist)) { // Don't export URI in the sandbox
			ntValue *uri = nt_value_new_string_utf8(misc->module, name);
			nt_value_decref(nt_value_set_recursive_utf8(misc->module, "module.uri", uri, ntPropAttrConstant, true));
			nt_value_decref(uri);
		}
		nt_value_decref(whitelist);

		assert(nt_private_set(misc->req->modules, name, misc->module, (ntFreeFunction) nt_value_decref));
	}
}

ntValue          *nt_require                   (ntValue *ctx, const char *name) {
	// Get the global and the ntRequire struct
	ntValue  *global = nt_value_get_global(ctx);
	ntRequire   *req = nt_value_private_get(global, NATUS_REQUIRE);
	if (!global || !req) {
		nt_value_decref(global);
		return NULL;
	}

	// Check to see if we've already loaded the module
	ntHookData hd = { name, req, ntRequireHookStepResolve, ctx, NULL };
	nt_private_foreach(req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
	if (hd.res) {
		nt_value_decref(global);
		hd.module = nt_value_incref(hd.res);
		goto modfound;
	}

	// Setup our import environment
	hd.module = prep_module_base(global, name);
	nt_value_decref(global);
	if (!hd.module) goto notfound;

	// Execute the pre-hooks
	hd.step = ntRequireHookStepLoad;
	nt_private_foreach(req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
	if (hd.res && nt_value_is_exception(hd.res)) {
		nt_value_decref(hd.module);
		return hd.res;
	} else if (!nt_value_is_undefined(hd.res)) {
		nt_value_decref(hd.res);
		goto modfound;
	}
	nt_value_decref(hd.module);

	// Module not found
	notfound:
		return nt_throw_exception(ctx, "ImportError", "%s not found!", name);

	// Module found
	modfound:
		hd.res    = nt_value_get_utf8(hd.module, "module");
		hd.module = nt_value_get_utf8(hd.module, "exports");
		nt_value_decref(nt_value_set_utf8(hd.module, "module", hd.res, ntPropAttrConstant));
		nt_value_decref(hd.res);

		// Loop through the module post hooks
		hd.step = ntRequireHookStepProcess;
		hd.res  = NULL;
		nt_private_foreach(req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
		nt_value_decref(hd.res);
		return hd.module;
}
