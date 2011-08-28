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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "private.h"
#include "require.h"

#define  _str(s) # s
#define __str(s) _str(s)

#define JS_REQUIRE_PREFIX "(function(exports, require, module) {\n"
#define JS_REQUIRE_SUFFIX "\n})"

typedef struct {
  natusValue *config;
  natusPrivate *hooks;
  natusPrivate *matchers;
  natusPrivate *dll;
  natusPrivate *modules;
} natusRequire;

static void
_natus_require_free(natusRequire *req)
{
  if (!req)
    return;
  natus_decref(req->config);
  natus_private_free(req->hooks);
  natus_private_free(req->matchers);
  natus_private_free(req->dll);
  natus_private_free(req->modules);
  free(req);
}

typedef struct {
  void *misc;
  natusFreeFunction free;
} reqPayload;

typedef struct {
  reqPayload head;
  natusRequireOriginMatcher func;
} reqOriginMatcher;

typedef struct {
  reqPayload head;
  natusRequireHook func;
} reqHook;

static void
_free_pl(reqPayload *pl)
{
  if (!pl)
    return;
  if (pl->misc && pl->free)
    pl->free(pl->misc);
  free(pl);
}

typedef struct {
  char *pat;
  const char *uri;
  bool match;
} loopData;

static void
_foreach_match(const char *name, reqOriginMatcher *om, loopData *ld)
{
  if (!om->func || ld->match)
    return;
  ld->match = om->func(ld->pat, ld->uri);
}

static char *
check_path(struct stat *st, const char *fmt, ...) {
  char *fn = NULL;

  va_list ap;
  va_start(ap, fmt);
  int sz = vasprintf(&fn, fmt, ap);
  va_end(ap);
  if (sz < 0)
  return NULL;

  char *rp = realpath(fn, NULL);
  free(fn);
  if (!rp)
  return NULL;

  if (stat(rp, st) == 0)
  return rp;
  free(rp);
  return NULL;
}

static natusValue*
internal_require(natusValue *ctx, natusRequireHookStep step, char *name, void *misc)
{
  if (step == natusRequireHookStepProcess)
    return NULL;

  natusRequire *req = natus_get_private(ctx, natusRequire*);
  if (!req)
    return NULL;

  natusValue *path = NULL;

  // If the path is a relative path, use the top of the evaluation stack
  if (name && name[0] == '.') {
    natusValue *stack = natus_get_private_name_value(ctx, NATUS_REQUIRE_STACK);
    long len = natus_as_long(natus_get_utf8(stack, "length"));

    natusValue *prfx = natus_get_index(stack, len > 0 ? len - 1 : 0);
    natus_decref(stack);
    if (natus_is_undefined(prfx)) {
      natus_decref(prfx);
      prfx = natus_new_string_utf8(ctx, ".");
    }

    const natusValue *items[2] =
      { prfx, NULL };
    path = natus_new_array_vector(ctx, items);
    natus_decref(prfx);

    // Otherwise use the normal path
  } else {
    natusValue *config = natus_require_get_config(ctx);
    if (!config)
      return NULL;

    path = natus_get_recursive_utf8(config, CFG_PATH);
    natus_decref(config);
  }

  struct stat st;
  long i, len = natus_as_long(natus_get_utf8(path, "length"));
  for (i = 0; i < len; i++) {
    natusValue *retval = NULL;
    char *prefix = natus_as_string_utf8(natus_get_index(path, i), NULL);

    // Check for native modules
    char *file = check_path(&st, "%s/%s%s", prefix, name, MODSUFFIX);
    if (file) {
      if (step == natusRequireHookStepResolve)
        goto out;

      void *dll = dlopen(file, RTLD_NOW | RTLD_GLOBAL);
      if (!dll) {
        retval = natus_throw_exception(ctx, NULL, "RequireError", dlerror());
        goto out;
      }

      retval = natus_new_object(ctx, NULL);
      natusRequireModuleInit load = (natusRequireModuleInit) dlsym(dll, "natus_module_init");
      if (!retval || !load || !load(retval)) {
        natus_decref(retval);
        dlclose(dll);
        retval = natus_throw_exception(ctx, NULL, "RequireError", "Module initialization failed!");
        goto out;
      }

      natus_private_push(req->dll, dll, (natusFreeFunction) dlclose);
      goto out;
    }

    // Check for javascript modules
    file = check_path(&st, "%s/%s.js", prefix, name);
    if (!file)
      file = check_path(&st, "%s/%s/__init__.js", prefix, name);
    if (file) {
      if (step == natusRequireHookStepResolve)
        goto out;

      // If we have a javascript text module
      FILE *modfile = fopen(file, "r");
      if (!modfile)
        goto out;

      // Read the file in, but wrapped in a function
      size_t len = strlen(JS_REQUIRE_PREFIX) + strlen(JS_REQUIRE_SUFFIX) + st.st_size + 1;
      char *jscript = calloc(len, sizeof(char));
      memset(jscript, 0, len);
      strcpy(jscript, JS_REQUIRE_PREFIX);
      if (fread(jscript + strlen(JS_REQUIRE_PREFIX), 1, st.st_size, modfile) != st.st_size) {
        fclose(modfile);
        free(jscript);
        retval = natus_throw_exception(ctx, NULL, "RequireError", "Error reading file!");
        goto out;
      }
      fclose(modfile);
      strcat(jscript, JS_REQUIRE_SUFFIX);

      // Evaluate the file
      natusValue *func = natus_evaluate_utf8(ctx, jscript, file, 0);
      free(jscript);
      if (natus_is_exception(func)) {
        retval = func;
        goto out;
      }

      // Create our arguments (exports, require, module)
      natusValue *module = natus_new_object(ctx, NULL);
      natusValue *exports = natus_new_object(ctx, NULL);
      natusValue *require = natus_get_utf8(ctx, "require");
      natusValue *modid = natus_new_string_utf8(ctx, name);
      strncpy(name, "file://", PATH_MAX);
      strncat(name, file, PATH_MAX - strlen("file://"));
      natusValue *moduri = natus_new_string_utf8(ctx, name);
      natus_decref(natus_set_utf8(module, "id", modid, natusPropAttrConstant));
      natus_decref(natus_set_utf8(module, "uri", moduri, natusPropAttrConstant));
      natus_decref(modid);
      natus_decref(moduri);

      // Call the function
      natusValue *res = natus_call(func, exports, exports, require, module, NULL);
      natus_decref(require);
      natus_decref(module);
      natus_decref(func);
      if (natus_is_exception(res)) {
        natus_decref(exports);
        retval = res;
        goto out;
      }

      // Function successfully executed, return the exports
      natus_decref(res);
      retval = exports;
      goto out;
    }

    free(prefix);
    continue;

  out:
    strncpy(name, "file://", PATH_MAX);
    strncat(name, file, PATH_MAX - strlen("file://"));
    natus_decref(path);
    free(prefix);
    free(file);
    return retval;
  }
  natus_decref(path);
  return NULL;
}

static natusValue *
require_js(natusValue *require, natusValue *ths, natusValue *arg)
{
  NATUS_CHECK_ARGUMENTS(arg, "s");

  // Get the required name argument
  char *modname = natus_as_string_utf8(natus_get_index(arg, 0), NULL);

  // Build our path and whitelist
  natusValue *cfg = natus_require_get_config(require);
  natusValue *path = natus_get_recursive_utf8(cfg, CFG_PATH);
  natusValue *whitelist = natus_get_recursive_utf8(cfg, CFG_WHITELIST);

  // Security check
  if (natus_is_array(whitelist)) {
    bool cleared = false;
    long i, len = natus_as_long(natus_get_utf8(whitelist, "length"));
    for (i = 0; !cleared && i < len; i++) {
      char *item = natus_as_string_utf8(natus_get_index(whitelist, i), NULL);
      cleared = !strcmp(modname, item);
      free(item);
    }
    if (!cleared) {
      natus_decref(cfg);
      natus_decref(path);
      natus_decref(whitelist);
      return natus_throw_exception(ths, NULL, "SecurityError", "Permission denied!");
    }
  }

  natusValue *mod = natus_require(require, modname);
  free(modname);
  natus_decref(cfg);
  natus_decref(path);
  natus_decref(whitelist);
  return mod;
}

bool
natus_require_init(natusValue *ctx, const char *config)
{
  natusValue *glb, *json = NULL, *arg = NULL, *cfg = NULL;
  bool rslt = false;

  // Parse the config
  if (!config)
    config = "{}";

  glb  = natus_get_global(ctx);
  if (natus_is_exception(glb))
    return false;

  json = natus_get_utf8(glb, "JSON");
  if (natus_is_exception(json))
    goto out;

  arg  = natus_new_string_utf8(glb, config);
  if (natus_is_exception(arg))
    goto out;

  cfg  = natus_call_new_utf8(json, "parse", arg, NULL);
  if (natus_is_exception(cfg))
    goto out;

  rslt = natus_require_init_value(ctx, cfg);

out:
  natus_decref(json);
  natus_decref(arg);
  natus_decref(cfg);
  return rslt;
}

bool
natus_require_init_value(natusValue *ctx, natusValue *config)
{
  natusValue *glb = NULL, *pth = NULL;
  natusRequire *req = NULL;

  // Get the global
  glb = natus_get_global(ctx);
  if (!glb)
    goto error;

  // Make sure we haven't already initialized a module loader
  // If not, initialize
  req = natus_get_private(glb, natusRequire*);
  if (req)
    return true;
  if (!(req = malloc(sizeof(natusRequire))))
    goto error;
  memset(req, 0, sizeof(natusRequire));
  req->config = natus_incref(config);
  req->hooks = natus_private_init(NULL, NULL);
  req->matchers = natus_private_init(NULL, NULL);
  req->dll = natus_private_init(NULL, NULL);
  req->modules = natus_private_init(NULL, NULL);
  if (!req->hooks || !req->matchers || !req->dll || !req->modules)
    goto error;

  // Add require function
  pth = natus_get_recursive_utf8(req->config, CFG_PATH);
  if (pth && natus_is_array(pth) &&
      natus_as_long(natus_get_utf8(pth, "length")) > 0) {
    // The stack is the one big exception to the layer boundary since it needs to be
    // updated by the natus_evalue() function.
    natusValue *stack = natus_new_array(glb, NULL);
    if (!natus_is_array(stack) ||
        !natus_set_private_name_value(glb, NATUS_REQUIRE_STACK, stack)) {
      natus_decref(stack);
      goto error;
    }
    natus_decref(stack);

    // Setup the require function
    natusValue *require = natus_new_function(glb, require_js, "require");
    if (!natus_is_function(require) ||
        !natus_as_bool(natus_set_utf8(glb, "require", require, natusPropAttrConstant))) {
      natus_decref(require);
      goto error;
    }

    // Add the paths variable if we are not in a sandbox
    natusValue *whitelist = natus_get_utf8(req->config, CFG_WHITELIST);
    if (!natus_is_array(whitelist))
      natus_decref(natus_set_utf8(require, "paths", pth, natusPropAttrConstant));
    natus_decref(whitelist);

    natus_decref(require);
  }
  natus_decref(pth);

  // Finish initialization
  if (!natus_set_private(glb, natusRequire*, req, (natusFreeFunction) _natus_require_free) ||
      !natus_require_hook_add(glb, "natus::InternalLoader", internal_require, NULL, NULL)) {
    natus_set_private(glb, natusRequire*, NULL, NULL);
    natus_set_private_name(glb, NATUS_REQUIRE_STACK, NULL, NULL);
    natus_del_utf8(glb, "require");
    return false;
  }
  return true;

error:
  _natus_require_free(req);
  natus_decref(pth);
  return false;
}

void
natus_require_free(natusValue *ctx)
{
  natusValue *glb = natus_get_global(ctx);
  natus_set_private(glb, natusRequire*, NULL, NULL);
  natus_set_private_name(glb, NATUS_REQUIRE_STACK, NULL, NULL);
}

natusValue *
natus_require_get_config(natusValue *ctx)
{
  // Get the global and the natusRequire struct
  natusValue *global = natus_get_global(ctx);
  natusRequire *req = natus_get_private(global, natusRequire*);
  return natus_incref(req ? req->config : NULL);
}

#define _do_set(ctx, field, name, item, free) \
  natusValue *glb = natus_get_global(ctx); \
  if (!glb) goto error; \
  natusRequire *req = (natusRequire*) natus_get_private(glb, natusRequire*); \
  if (!req) goto error; \
  return natus_private_set(req->field, name, item, ((natusFreeFunction) free)); \
  error: \
    if (item) free((reqPayload*) item); \
    return false;

bool
natus_require_hook_add(natusValue *ctx, const char *name, natusRequireHook func, void *misc, natusFreeFunction free)
{
  reqHook *hook = malloc(sizeof(reqHook));
  if (!hook)
    return false;
  hook->head.misc = misc;
  hook->head.free = free;
  hook->func = func;
  _do_set(ctx, hooks, name, hook, _free_pl);
}

bool
natus_require_hook_del(natusValue *ctx, const char *name)
{
  _do_set(ctx, hooks, name, NULL, _free_pl);
}

bool
natus_require_origin_matcher_add(natusValue *ctx, const char *name, natusRequireOriginMatcher func, void *misc, natusFreeFunction free)
{
  reqOriginMatcher *match = malloc(sizeof(reqOriginMatcher));
  if (!match)
    return false;
  match->head.misc = misc;
  match->head.free = free;
  match->func = func;
  _do_set(ctx, matchers, name, match, _free_pl);
}

bool
natus_require_origin_matcher_del(natusValue *ctx, const char *name)
{
  _do_set(ctx, matchers, name, NULL, _free_pl);
}

bool
natus_require_origin_permitted(natusValue *ctx, const char *uri)
{
  // Get the global
  natusValue *glb = natus_get_global(ctx);
  if (!glb)
    return true;

  // Get the require structure
  natusRequire *req = natus_get_private(glb, natusRequire*);
  if (!req)
    return true;

  // Get the whitelist and blacklist
  natusValue *whitelist = natus_get_recursive_utf8(req->config, CFG_ORIGINS_WHITELIST);
  natusValue *blacklist = natus_get_recursive_utf8(req->config, CFG_ORIGINS_BLACKLIST);
  if (!natus_is_array(whitelist)) {
    natus_decref(blacklist);
    natus_decref(whitelist);
    return true;
  }

  // Check to see if the URI is on the approved whitelist
  int i;
  loopData ld =
    { .match = false, .uri = uri };
  long len = natus_as_long(natus_get_utf8(whitelist, "length"));
  for (i = 0; !ld.match && i < len; i++) {
    ld.pat = natus_as_string_utf8(natus_get_index(whitelist, i), NULL);
    if (!ld.pat)
      continue;
    natus_private_foreach(req->matchers, false, (natusPrivateForeach) _foreach_match, &ld);
    free(ld.pat);
  }
  natus_decref(whitelist);

  // If the URI matched the whitelist, check the blacklist
  if (ld.match && natus_is_array(blacklist)) {
    ld.match = false;
    len = natus_as_long(natus_get_utf8(blacklist, "length"));
    for (i = 0; !ld.match && i < len; i++) {
      ld.pat = natus_as_string_utf8(natus_get_index(blacklist, i), NULL);
      if (!ld.pat)
        continue;
      natus_private_foreach(req->matchers, false, (natusPrivateForeach) _foreach_match, &ld);
      free(ld.pat);
    }
    ld.match = !ld.match;
  }
  natus_decref(blacklist);

  return ld.match;
}

typedef struct {
  const char *name;
  natusRequire *req;
  natusRequireHookStep step;
  natusValue *ctx;
  natusValue *res;
} natusHookData;

static void
do_hooks(const char *hookname, reqHook *hook, natusHookData *misc)
{
  if (!misc || misc->res)
    return;
  char name[PATH_MAX];
  strcpy(name, misc->name);

  // Call the hook
  misc->res = hook->func(misc->ctx, misc->step, name, hook->head.misc);

  // If the step is resolve, we're just checking for cache names
  if (misc->step == natusRequireHookStepResolve) {
    natus_decref(misc->res);

    // If the name was found in the cache, return it
    misc->res = natus_private_get(misc->req->modules, name);
    return;
  }

  // Return immediately on an exception
  if (natus_is_exception(misc->res))
    return;

  // If a module was found, cache it according to the (possibly modified) name
  if (misc->step == natusRequireHookStepLoad) {
    // Ignore hooks that return invalid values
    if (!natus_is_object(misc->res)) {
      natus_decref(misc->res);
      misc->res = NULL;
      return;
    }

    // Set the module id
    natusValue *modname = natus_new_string_utf8(misc->ctx, misc->name);
    natus_decref(natus_set_recursive_utf8(misc->res, "module.id", modname, natusPropAttrConstant, true));
    natus_decref(modname);

    // Set the module uri (if not sandboxed)
    natusValue *whitelist = natus_get_utf8(misc->req->config, CFG_WHITELIST);
    if (!natus_is_array(whitelist)) { // Don't export URI in the sandbox
      natusValue *uri = natus_new_string_utf8(misc->res, name);
      natus_decref(natus_set_recursive_utf8(misc->res, "module.uri", uri, natusPropAttrConstant, true));
      natus_decref(uri);
    }
    natus_decref(whitelist);

    // Store the module in the cache
    assert(natus_private_set(misc->req->modules, name, natus_incref(misc->res), (natusFreeFunction) natus_decref));
  }
}

natusValue *
natus_require(natusValue *ctx, const char *name)
{
  // Get the global and the natusRequire struct
  natusValue *global = natus_get_global(ctx);
  natusRequire *req = natus_get_private(global, natusRequire*);
  if (!global || !req)
    return NULL;

  // Check to see if we've already loaded the module (resolve step)
  natusHookData hd =
    { name, req, natusRequireHookStepResolve, global, NULL };
  natus_private_foreach(req->hooks, true, (natusPrivateForeach) do_hooks, &hd);
  if (hd.res) {
    if (!natus_is_exception(hd.res))
      goto modfound;
    return hd.res;
  }

  // Load the module (load step)
  hd.step = natusRequireHookStepLoad;
  natus_private_foreach(req->hooks, true, (natusPrivateForeach) do_hooks, &hd);
  if (hd.res) {
    if (!natus_is_exception(hd.res))
      goto modfound;
    return hd.res;
  }
  natus_decref(hd.res);

  // Module not found
  return natus_throw_exception(ctx, NULL, "RequireError", "Module %s not found!", name);

  // Module found
modfound:
  hd.step = natusRequireHookStepProcess;
  hd.ctx = hd.res;
  hd.res = NULL;
  natus_private_foreach(req->hooks, true, (natusPrivateForeach) do_hooks, &hd);
  natus_decref(hd.res);
  return hd.ctx;
}
