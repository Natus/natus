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

#define _GNU_SOURCE
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

#define NATUS_REQUIRE_STACK   "natus::Require::Stack"
#define CFG_PATH              "natus.require.path"
#define CFG_WHITELIST         "natus.require.whitelist"
#define CFG_ORIGINS_WHITELIST "natus.origins.whitelist"
#define CFG_ORIGINS_BLACKLIST "natus.origins.blacklist"

#include "value.h"
#include "misc.h"
#include "private.h"
#include "require.h"

#define  _str(s) # s
#define __str(s) _str(s)

#define JS_REQUIRE_PREFIX "(function(exports, require, module) {\n"
#define JS_REQUIRE_SUFFIX "\n})"

typedef struct {
	ntValue *config;
	ntPrivate *hooks;
	ntPrivate *matchers;
	ntPrivate *dll;
	ntPrivate *modules;
} ntRequire;

static void _nt_require_free (ntRequire *req) {
	if (!req)
		return;
	nt_value_decref (req->config);
	nt_private_free (req->hooks);
	nt_private_free (req->matchers);
	nt_private_free (req->dll);
	nt_private_free (req->modules);
	free (req);
}

typedef struct {
	void *misc;
	ntFreeFunction free;
} reqPayload;

typedef struct {
	reqPayload head;
	ntRequireOriginMatcher func;
} reqOriginMatcher;

typedef struct {
	reqPayload head;
	ntRequireHook func;
} reqHook;

static void _free_pl (reqPayload *pl) {
	if (!pl)
		return;
	if (pl->misc && pl->free)
		pl->free (pl->misc);
	free (pl);
}

typedef struct {
	char *pat;
	const char *uri;
	bool match;
} loopData;

static void _foreach_match (const char *name, reqOriginMatcher *om, loopData *ld) {
	if (!om->func || ld->match)
		return;
	ld->match = om->func (ld->pat, ld->uri);
}

static char *check_path (struct stat *st, const char *fmt, ...) {
	char *fn = NULL;

	va_list ap;
	va_start(ap, fmt);
	int sz = vasprintf (&fn, fmt, ap);
	va_end(ap);
	if (sz < 0)
		return NULL;

	char *rp = realpath (fn, NULL);
	free (fn);
	if (!rp)
		return NULL;

	if (stat (rp, st) == 0)
		return rp;
	free (rp);
	return NULL;
}

static ntValue* internal_require (ntValue *ctx, ntRequireHookStep step, char *name, void *misc) {
	if (step == ntRequireHookStepProcess)
		return NULL;

	ntRequire *req = nt_value_get_private(ctx, ntRequire*);
	if (!req)
		return NULL;

	ntValue *path = NULL;

	// If the path is a relative path, use the top of the evaluation stack
	if (name && name[0] == '.') {
		ntValue *stack = nt_value_get_private_name_value (ctx, NATUS_REQUIRE_STACK);
		long len = nt_value_as_long (nt_value_get_utf8 (stack, "length"));

		ntValue *prfx = nt_value_get_index (stack, len > 0 ? len - 1 : 0);
		nt_value_decref (stack);
		if (nt_value_is_undefined (prfx)) {
			nt_value_decref(prfx);
			prfx = nt_value_new_string_utf8 (ctx, ".");
		}

		const ntValue *items[2] = { prfx, NULL };
		path = nt_value_new_array_vector (ctx, items);
		nt_value_decref (prfx);

		// Otherwise use the normal path
	} else {
		ntValue *config = nt_require_get_config (ctx);
		if (!config)
			return NULL;

		path = nt_value_get_recursive_utf8 (config, CFG_PATH);
		nt_value_decref (config);
	}

	struct stat st;
	long i, len = nt_value_as_long (nt_value_get_utf8 (path, "length"));
	for (i = 0; i < len ; i++) {
		ntValue *retval = NULL;
		char *prefix = nt_value_as_string_utf8 (nt_value_get_index (path, i), NULL);

		// Check for native modules
		char *file = check_path (&st, "%s/%s%s", prefix, name, MODSUFFIX);
		if (file) {
			if (step == ntRequireHookStepResolve)
				goto out;

			void *dll = dlopen (file, RTLD_NOW | RTLD_GLOBAL);
			if (!dll) {
				retval = nt_throw_exception (ctx, NULL, "RequireError", dlerror ());
				goto out;
			}

			retval = nt_value_new_object (ctx, NULL);
			ntRequireModuleInit load = (ntRequireModuleInit) dlsym (dll, "natus_module_init");
			if (!retval || !load || !load (retval)) {
				nt_value_decref (retval);
				dlclose (dll);
				retval = nt_throw_exception (ctx, NULL, "RequireError", "Module initialization failed!");
				goto out;
			}

			nt_private_push (req->dll, dll, (ntFreeFunction) dlclose);
			goto out;
		}

		// Check for javascript modules
		file = check_path (&st, "%s/%s.js", prefix, name);
		if (!file)
			file = check_path (&st, "%s/%s/__init__.js", prefix, name);
		if (file) {
			if (step == ntRequireHookStepResolve)
				goto out;

			// If we have a javascript text module
			FILE *modfile = fopen (file, "r");
			if (!modfile)
				goto out;

			// Read the file in, but wrapped in a function
			size_t len = strlen (JS_REQUIRE_PREFIX) + strlen (JS_REQUIRE_SUFFIX) + st.st_size + 1;
			char *jscript = calloc (len, sizeof(char));
			memset (jscript, 0, len);
			strcpy (jscript, JS_REQUIRE_PREFIX);
			if (fread (jscript + strlen (JS_REQUIRE_PREFIX), 1, st.st_size, modfile) != st.st_size) {
				fclose (modfile);
				free (jscript);
				retval = nt_throw_exception (ctx, NULL, "RequireError", "Error reading file!");
				goto out;
			}
			fclose (modfile);
			strcat (jscript, JS_REQUIRE_SUFFIX);

			// Evaluate the file
			ntValue *func = nt_value_evaluate_utf8 (ctx, jscript, file, 0);
			free (jscript);
			if (nt_value_is_exception (func)) {
				retval = func;
				goto out;
			}

			// Create our arguments (exports, require, module)
			ntValue *module = nt_value_new_object (ctx, NULL);
			ntValue *exports = nt_value_new_object (ctx, NULL);
			ntValue *require = nt_value_get_utf8 (ctx, "require");
			ntValue *modid = nt_value_new_string_utf8 (ctx, name);
			strncpy (name, "file://", PATH_MAX);
			strncat (name, file, PATH_MAX - strlen ("file://"));
			ntValue *moduri = nt_value_new_string_utf8 (ctx, name);
			nt_value_decref (nt_value_set_utf8 (module, "id", modid, ntPropAttrConstant));
			nt_value_decref (nt_value_set_utf8 (module, "uri", moduri, ntPropAttrConstant));
			nt_value_decref (modid);
			nt_value_decref (moduri);

			// Convert arguments to an array
			const ntValue *argv[] = { exports, require, module, NULL };
			ntValue *args = nt_value_new_array_vector (ctx, argv);
			nt_value_decref (require);
			nt_value_decref (module);

			// Call the function
			ntValue *res = nt_value_call (func, exports, args);
			nt_value_decref (func);
			nt_value_decref (args);
			if (nt_value_is_exception (res)) {
				nt_value_decref (exports);
				retval = res;
				goto out;
			}

			// Function successfully executed, return the exports
			nt_value_decref (res);
			retval = exports;
			goto out;
		}

		free (prefix);
		continue;

		out:
			strncpy (name, "file://", PATH_MAX);
			strncat (name, file, PATH_MAX - strlen ("file://"));
			nt_value_decref (path);
			free (prefix);
			free (file);
			return retval;
	}
	nt_value_decref (path);
	return NULL;
}

static ntValue *require_js (ntValue *require, ntValue *ths, ntValue *arg) {
	NT_CHECK_ARGUMENTS(arg, "s");

	// Get the required name argument
	char *modname = nt_value_as_string_utf8 (nt_value_get_index (arg, 0), NULL);

	// Build our path and whitelist
	ntValue *cfg = nt_require_get_config (require);
	ntValue *path = nt_value_get_recursive_utf8 (cfg, CFG_PATH);
	ntValue *whitelist = nt_value_get_recursive_utf8 (cfg, CFG_WHITELIST);

	// Security check
	if (nt_value_is_array (whitelist)) {
		bool cleared = false;
		long i, len = nt_value_as_long (nt_value_get_utf8 (whitelist, "length"));
		for (i = 0; !cleared && i < len ; i++) {
			char *item = nt_value_as_string_utf8 (nt_value_get_index (whitelist, i), NULL);
			cleared = !strcmp (modname, item);
			free (item);
		}
		if (!cleared) {
			nt_value_decref (cfg);
			nt_value_decref (path);
			nt_value_decref (whitelist);
			return nt_throw_exception (ths, NULL, "SecurityError", "Permission denied!");
		}
	}

	ntValue *mod = nt_require (require, modname);
	free (modname);
	nt_value_decref (cfg);
	nt_value_decref (path);
	nt_value_decref (whitelist);
	return mod;
}

bool nt_require_init (ntValue *ctx, const char *config) {
	// Parse the config
	if (!config)
		config = "{}";
	ntValue *cfg = nt_from_json_utf8 (ctx, config, strlen (config));
	if (nt_value_is_exception (cfg)) {
		nt_value_decref (cfg);
		return false;
	}

	bool rslt = nt_require_init_value (ctx, cfg);
	nt_value_decref (cfg);
	return rslt;
}

bool nt_require_init_value (ntValue *ctx, ntValue *config) {
	ntValue *glb = NULL, *pth = NULL;
	ntRequire *req = NULL;

	// Get the global
	glb = nt_value_get_global (ctx);
	if (!glb)
		goto error;

	// Make sure we haven't already initialized a module loader
	// If not, initialize
	req = nt_value_get_private(glb, ntRequire*);
	if (req)
		return true;
	if (!(req = malloc (sizeof(ntRequire))))
		goto error;
	memset (req, 0, sizeof(ntRequire));
	req->config = nt_value_incref (config);
	req->hooks = nt_private_init ();
	req->matchers = nt_private_init ();
	req->dll = nt_private_init ();
	req->modules = nt_private_init ();
	if (!req->hooks || !req->matchers || !req->dll || !req->modules)
		goto error;

	// Add require function
	pth = nt_value_get_recursive_utf8 (req->config, CFG_PATH);
	if (pth && nt_value_is_array (pth) && nt_value_as_long (nt_value_get_utf8 (pth, "length")) > 0) {
		// The stack is the one big exception to the layer boundary since it needs to be
		// updated by the nt_value_evalue() function.
		ntValue *stack = nt_value_new_array (glb, NULL);
		if (!nt_value_is_array (stack) || !nt_value_set_private_name_value (glb, NATUS_REQUIRE_STACK, stack)) {
			nt_value_decref (stack);
			goto error;
		}
		nt_value_decref (stack);

		// Setup the require function
		ntValue *require = nt_value_new_function (glb, require_js, "require");
		if (!nt_value_is_function (require) || !nt_value_as_bool (nt_value_set_utf8 (glb, "require", require, ntPropAttrConstant))) {
			nt_value_decref (require);
			goto error;
		}

		// Add the paths variable if we are not in a sandbox
		ntValue *whitelist = nt_value_get_utf8 (req->config, CFG_WHITELIST);
		if (!nt_value_is_array (whitelist))
			nt_value_decref (nt_value_set_utf8 (require, "paths", pth, ntPropAttrConstant));
		nt_value_decref (whitelist);

		nt_value_decref (require);
	}
	nt_value_decref (pth);

	// Finish initialization
	if (!nt_value_set_private(glb, ntRequire*, req, (ntFreeFunction) _nt_require_free) || !nt_require_hook_add (glb, "natus::InternalLoader", internal_require, NULL, NULL)) {
		nt_value_set_private(glb, ntRequire*, NULL, NULL);
		nt_value_set_private_name (glb, NATUS_REQUIRE_STACK, NULL, NULL);
		nt_value_del_utf8 (glb, "require");
		return false;
	}
	return true;

	error: _nt_require_free (req);
	nt_value_decref (pth);
	return false;
}

void nt_require_free (ntValue *ctx) {
	ntValue *glb = nt_value_get_global (ctx);
	nt_value_set_private(glb, ntRequire*, NULL, NULL);
	nt_value_set_private_name (glb, NATUS_REQUIRE_STACK, NULL, NULL);
}

ntValue *nt_require_get_config (ntValue *ctx) {
	// Get the global and the ntRequire struct
	ntValue *global = nt_value_get_global (ctx);
	ntRequire *req = nt_value_get_private(global, ntRequire*);
	return nt_value_incref (req ? req->config : NULL);
}

#define _do_set(ctx, field, name, item, free) \
	ntValue *glb = nt_value_get_global(ctx); \
	if (!glb) goto error; \
	ntRequire *req = (ntRequire*) nt_value_get_private(glb, ntRequire*); \
	if (!req) goto error; \
	return nt_private_set(req->field, name, item, ((ntFreeFunction) free)); \
	error: \
		if (item) free((reqPayload*) item); \
		return false;

bool nt_require_hook_add (ntValue *ctx, const char *name, ntRequireHook func, void *misc, ntFreeFunction free) {
	reqHook *hook = malloc (sizeof(reqHook));
	if (!hook)
		return false;
	hook->head.misc = misc;
	hook->head.free = free;
	hook->func = func;
	_do_set(ctx, hooks, name, hook, _free_pl);
}

bool nt_require_hook_del (ntValue *ctx, const char *name) {
	_do_set(ctx, hooks, name, NULL, _free_pl);
}

bool nt_require_origin_matcher_add (ntValue *ctx, const char *name, ntRequireOriginMatcher func, void *misc, ntFreeFunction free) {
	reqOriginMatcher *match = malloc (sizeof(reqOriginMatcher));
	if (!match)
		return false;
	match->head.misc = misc;
	match->head.free = free;
	match->func = func;
	_do_set(ctx, matchers, name, match, _free_pl);
}

bool nt_require_origin_matcher_del (ntValue *ctx, const char *name) {
	_do_set(ctx, matchers, name, NULL, _free_pl);
}

bool nt_require_origin_permitted (ntValue *ctx, const char *uri) {
	// Get the global
	ntValue *glb = nt_value_get_global (ctx);
	if (!glb)
		return true;

	// Get the require structure
	ntRequire *req = nt_value_get_private(glb, ntRequire*);
	if (!req)
		return true;

	// Get the whitelist and blacklist
	ntValue *whitelist = nt_value_get_recursive_utf8 (req->config, CFG_ORIGINS_WHITELIST);
	ntValue *blacklist = nt_value_get_recursive_utf8 (req->config, CFG_ORIGINS_BLACKLIST);
	if (!nt_value_is_array (whitelist)) {
		nt_value_decref (blacklist);
		nt_value_decref (whitelist);
		return true;
	}

	// Check to see if the URI is on the approved whitelist
	int i;
	loopData ld = { .match = false, .uri = uri };
	long len = nt_value_as_long (nt_value_get_utf8 (whitelist, "length"));
	for (i = 0; !ld.match && i < len ; i++) {
		ld.pat = nt_value_as_string_utf8 (nt_value_get_index (whitelist, i), NULL);
		if (!ld.pat)
			continue;
		nt_private_foreach (req->matchers, false, (ntPrivateForeach) _foreach_match, &ld);
		free (ld.pat);
	}
	nt_value_decref (whitelist);

	// If the URI matched the whitelist, check the blacklist
	if (ld.match && nt_value_is_array (blacklist)) {
		ld.match = false;
		len = nt_value_as_long (nt_value_get_utf8 (blacklist, "length"));
		for (i = 0; !ld.match && i < len ; i++) {
			ld.pat = nt_value_as_string_utf8 (nt_value_get_index (blacklist, i), NULL);
			if (!ld.pat)
				continue;
			nt_private_foreach (req->matchers, false, (ntPrivateForeach) _foreach_match, &ld);
			free (ld.pat);
		}
		ld.match = !ld.match;
	}
	nt_value_decref (blacklist);

	return ld.match;
}

typedef struct {
	const char *name;
	ntRequire *req;
	ntRequireHookStep step;
	ntValue *ctx;
	ntValue *res;
} ntHookData;

static void do_hooks (const char *hookname, reqHook *hook, ntHookData *misc) {
	if (!misc || misc->res)
		return;
	char name[PATH_MAX];
	strcpy (name, misc->name);

	// Call the hook
	misc->res = hook->func (misc->ctx, misc->step, name, hook->head.misc);

	// If the step is resolve, we're just checking for cache names
	if (misc->step == ntRequireHookStepResolve) {
		nt_value_decref (misc->res);

		// If the name was found in the cache, return it
		misc->res = nt_private_get (misc->req->modules, name);
		return;
	}

	// Return immediately on an exception
	if (nt_value_is_exception (misc->res))
		return;

	// If a module was found, cache it according to the (possibly modified) name
	if (misc->step == ntRequireHookStepLoad) {
		// Ignore hooks that return invalid values
		if (!nt_value_is_object (misc->res)) {
			nt_value_decref (misc->res);
			misc->res = NULL;
			return;
		}

		// Set the module id
		ntValue *modname = nt_value_new_string_utf8 (misc->ctx, misc->name);
		nt_value_decref (nt_value_set_recursive_utf8 (misc->res, "module.id", modname, ntPropAttrConstant, true));
		nt_value_decref (modname);

		// Set the module uri (if not sandboxed)
		ntValue *whitelist = nt_value_get_utf8 (misc->req->config, CFG_WHITELIST);
		if (!nt_value_is_array (whitelist)) { // Don't export URI in the sandbox
			ntValue *uri = nt_value_new_string_utf8 (misc->res, name);
			nt_value_decref (nt_value_set_recursive_utf8 (misc->res, "module.uri", uri, ntPropAttrConstant, true));
			nt_value_decref (uri);
		}
		nt_value_decref (whitelist);

		// Store the module in the cache
		assert(nt_private_set(misc->req->modules, name, nt_value_incref(misc->res), (ntFreeFunction) nt_value_decref));
	}
}

ntValue *nt_require (ntValue *ctx, const char *name) {
	// Get the global and the ntRequire struct
	ntValue *global = nt_value_get_global (ctx);
	ntRequire *req = nt_value_get_private(global, ntRequire*);
	if (!global || !req)
		return NULL;

	// Check to see if we've already loaded the module (resolve step)
	ntHookData hd = { name, req, ntRequireHookStepResolve, global, NULL };
	nt_private_foreach (req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
	if (hd.res) {
		if (!nt_value_is_exception (hd.res))
			goto modfound;
		return hd.res;
	}

	// Load the module (load step)
	hd.step = ntRequireHookStepLoad;
	nt_private_foreach (req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
	if (hd.res) {
		if (!nt_value_is_exception (hd.res))
			goto modfound;
		return hd.res;
	}
	nt_value_decref (hd.res);

	// Module not found
	return nt_throw_exception (ctx, NULL, "RequireError", "Module %s not found!", name);

	// Module found
	modfound: hd.step = ntRequireHookStepProcess;
	hd.ctx = hd.res;
	hd.res = NULL;
	nt_private_foreach (req->hooks, true, (ntPrivateForeach) do_hooks, &hd);
	nt_value_decref (hd.res);
	return hd.ctx;
}
