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

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-require.h>

#include <libmem.h>

#include "evil.h"

#define SCRSUFFIX  ".js"
#define PKGSUFFIX  "/__init__" SCRSUFFIX
#define URIPREFIX  "file://"
#define NFSUFFIX   " not found!"

#define NATUS_REQUIRE_CONFIG  "natus::Require::Config"
#define NATUS_REQUIRE_CACHE   "natus::Require::Cache"
#define CFG_PATH              "natus.require.path"
#define CFG_WHITELIST         "natus.require.whitelist"
#define CFG_ORIGINS_WHITELIST "natus.origins.whitelist"
#define CFG_ORIGINS_BLACKLIST "natus.origins.blacklist"

#define  _str(s) # s
#define __str(s) _str(s)

#define JS_REQUIRE_PREFIX "(function(exports, require, module) {\n"
#define JS_REQUIRE_SUFFIX "\n})"

typedef struct reqOriginMatcher reqOriginMatcher;
struct reqOriginMatcher {
  reqOriginMatcher         *next;
  void                     *misc;
  natusFreeFunction         free;
  natusRequireOriginMatcher func;
};

typedef struct reqHook reqHook;
struct reqHook {
  reqHook          *next;
  void             *misc;
  natusFreeFunction free;
  natusRequireHook  func;
};

typedef struct {
  reqHook          *hooks;
  reqOriginMatcher *matchers;
} natusRequire;

static natusRequire *
get_require(natusValue *ctx);

static void
dll_dtor(void **dll)
{
  if (!dll || !*dll)
    return;
  dlclose(*dll);
}

static void
require_free(natusRequire *req)
{
  mem_free(req);
}

static void
hook_dtor(reqHook *hook)
{
  if (hook && hook->misc && hook->free)
    hook->free(hook->misc);
}

static void
originmatcher_dtor(reqOriginMatcher *om)
{
  if (om && om->misc && om->free)
    om->free(om->misc);
}

typedef struct {
  char *pat;
  const char *uri;
  bool match;
} loopData;

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

static natusValue *
internal_require_resolve(natusValue *ctx, natusValue *name)
{
  natusValue *path = NULL, *uris = NULL;

  char *modname = natus_to_string_utf8(name, NULL);
  if (!modname)
    return NULL;

  uris = natus_new_array(ctx, NULL);
  if (!natus_is_array(uris)) {
    free(modname);
    return NULL;
  }

  // If the path is a relative path, use the top of the evaluation stack
  if (modname && modname[0] == '.') {
    natusValue *stack = natus_get_private_name_value(ctx, NATUS_EVALUATION_STACK);
    long len = natus_as_long(natus_get_utf8(stack, "length"));

    natusValue *prfx = natus_get_index(stack, len > 0 ? len - 1 : 0);
    natus_decref(stack);
    if (natus_is_undefined(prfx)) {
      natus_decref(prfx);
      prfx = natus_new_string_utf8(ctx, ".");
    }

    path = natus_new_array(ctx, prfx, NULL);
    natus_decref(prfx);

  // Otherwise use the normal path
  } else {
    natusValue *config = natus_require_get_config(ctx);
    if (!config) {
      free(modname);
      natus_decref(uris);
      return NULL;
    }

    path = natus_get_recursive_utf8(config, CFG_PATH);
    natus_decref(config);
  }

  struct stat st;
  long i, len = natus_as_long(natus_get_utf8(path, "length"));
  for (i = 0; i < len; i++) {
    char *prefix = natus_as_string_utf8(natus_get_index(path, i), NULL);

    // Check for native modules
    char *file = check_path(&st, "%s/%s%s", prefix, modname, MODSUFFIX);
    if (!file) {
      // Check for javascript modules
      file = check_path(&st, "%s/%s.js", prefix, modname);
      if (!file)
        file = check_path(&st, "%s/%s/__init__.js", prefix, modname);
    }

    if (file) {
      natusValue *tmp = natus_new_string(ctx, "file://%s", file);
      if (!natus_is_exception(tmp))
        natus_push(uris, tmp);
      natus_decref(tmp);
    }

    free(prefix);
  }

  natus_decref(path);
  free(modname);
  return uris;
}

static natusValue *
internal_require_native(natusValue *ctx, natusValue *uri, const char *file)
{
  natusRequire *req = get_require(ctx);
  if (!req)
    return NULL;

  void **dll = mem_new_zero(req, void*);
  if (!dll) /* OOM */
    return NULL;

  *dll = dlopen(file, RTLD_NOW | RTLD_GLOBAL);
  if (!*dll) {
    mem_free(dll);
    return natus_throw_exception(ctx, NULL, "RequireError", dlerror());
  }
  mem_destructor_set(dll, dll_dtor);

  natusValue *module = natus_new_object(ctx, NULL);
  natusRequireModuleInit load = (natusRequireModuleInit) dlsym(dll, "natus_module_init");
  if (!module || !load || !load(module)) {
    natus_decref(module);
    mem_free(dll);
    return natus_throw_exception(ctx, NULL, "RequireError", "Module initialization failed!");
  }

  return module;
}

static natusValue *
internal_require_javascript(natusValue *ctx, natusValue *name, natusValue *uri, const char *file)
{
  struct stat st;

  // If we have a javascript text module
  FILE *modfile = fopen(file, "r");
  if (!modfile)
   return NULL;

  if (fstat(fileno(modfile), &st) != 0) {
    fclose(modfile);
    return NULL;
  }

  // Allocate our buffer
  char *jscript = mem_new_array_zero(NULL, char, st.st_size + 1);
  if (!jscript) {
    fclose(modfile);
    return NULL;
  }

  // Read the file
  size_t bytesread = 0;
  while (bytesread < st.st_size) {
    size_t br = fread(jscript + bytesread, 1,
                      st.st_size - bytesread,
                      modfile);
    if (br == 0 && (ferror(modfile) || feof(modfile))) {
      mem_free(jscript);
      fclose(modfile);
      return natus_throw_exception(ctx, NULL, "RequireError", "Error reading file!");
    }

    bytesread += br;
  }
  fclose(modfile);

  // Convert to a natus string and wrap in the function header/footer
  natusValue *javascript = natus_new_string(ctx, "%s%s%s",
                                            JS_REQUIRE_PREFIX,
                                            jscript,
                                            JS_REQUIRE_SUFFIX);
  mem_free(jscript);

  // Evaluate the file
  natusValue *func = natus_evaluate(ctx, javascript, uri, 0);
  natus_decref(javascript);
  if (natus_is_exception(func))
   return func;

  // Create our arguments (exports, require, module)
  natusValue *module = natus_new_object(ctx, NULL);
  natusValue *exports = natus_new_object(ctx, NULL);
  natusValue *require = natus_get_utf8(ctx, "require");
  natus_decref(natus_set_utf8(module, "id", name, natusPropAttrConstant));
  natus_decref(natus_set_utf8(module, "uri", uri, natusPropAttrConstant));

  // Call the function
  natusValue *res = natus_call(func, exports, exports, require, module, NULL);
  natus_decref(require);
  natus_decref(module);
  natus_decref(func);
  if (natus_is_exception(res)) {
   natus_decref(exports);
   return res;
  }

  // Function successfully executed, return the exports
  natus_decref(res);
  return exports;
}

static bool
endswith(const char *haystack, const char *needle)
{
  if (!haystack || !needle)
    return false;

  size_t hlen = strlen(haystack);
  size_t nlen = strlen(needle);

  if (hlen < nlen)
    return false;

  const char *loc = strstr(haystack, needle);
  if (!loc)
    return false;

  return (loc - haystack == hlen - nlen);
}

static natusValue *
internal_require(natusValue *ctx, natusRequireHookStep step,
                 natusValue *name, natusValue *uri, void *misc)
{
  if (step == natusRequireHookStepResolve)
    return internal_require_resolve(ctx, name);

  if (step != natusRequireHookStepLoad)
    return NULL;

  char *file = natus_to_string_utf8(uri, NULL);
  if (!file)
    return NULL;

  if (strstr(file, "file://") == file) {
    memmove(file, file + strlen("file://"), strlen(file) - strlen("file://"));
    file[strlen(file) - strlen("file://")] = '\0';
  }

  natusValue *rslt = NULL;
  if (endswith(file, MODSUFFIX))
    rslt = internal_require_native(ctx, uri, file);
  else if (endswith(file, SCRSUFFIX))
    rslt = internal_require_javascript(ctx, name, uri, file);

  free(file);
  return rslt;
}

static natusRequire *
get_require(natusValue *ctx)
{
  natusValue *global = natus_get_global(ctx);
  if (!global)
    return NULL;

  // If a require struct doesn't exist yet, create it
  natusRequire *req = natus_get_private(global, natusRequire*);
  if (!req) {
    req = mem_new_zero(NULL, natusRequire);
    if (!req)
      return NULL;
    if (!natus_set_private(global, natusRequire*, req,
                           (natusFreeFunction) require_free) ||
        !natus_require_hook_add(global, "natus::InternalLoader",
                                internal_require, NULL, NULL)) {
      natus_set_private(global, natusRequire*, NULL, NULL);
      return NULL;
    }
  }

  return req;
}

static natusValue *
require_js(natusValue *require, natusValue *ths, natusValue *arg)
{
  NATUS_CHECK_ARGUMENTS(arg, "s");

  // Get the required name argument
  natusValue *name = natus_get_index(arg, 0);

  // Build our path and whitelist
  natusValue *cfg = natus_require_get_config(require);
  natusValue *path = natus_get_recursive_utf8(cfg, CFG_PATH);
  natusValue *whitelist = natus_get_recursive_utf8(cfg, CFG_WHITELIST);

  // Security check
  if (natus_is_array(whitelist)) {
    bool cleared = false;
    long i, len = natus_as_long(natus_get_utf8(whitelist, "length"));
    for (i = 0; !cleared && i < len; i++) {
      natusValue *item = natus_get_index(whitelist, i);
      cleared = natus_equals_strict(name, item);
      natus_decref(item);
    }
    if (!cleared) {
      natus_decref(cfg);
      natus_decref(path);
      natus_decref(whitelist);
      return natus_throw_exception(ths, NULL, "SecurityError", "Permission denied!");
    }
  }

  natusValue *mod = natus_require(require, name);
  natus_decref(name);
  natus_decref(cfg);
  natus_decref(path);
  natus_decref(whitelist);
  return mod;
}

bool
natus_require_expose(natusValue *ctx, const char *config)
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

  rslt = natus_require_expose_value(ctx, cfg);

out:
  natus_decref(json);
  natus_decref(arg);
  natus_decref(cfg);
  return rslt;
}

bool
natus_require_expose_value(natusValue *ctx, natusValue *config)
{
  natusValue *glb = NULL, *pth = NULL, *cache;

  // Get the global
  glb = natus_get_global(ctx);
  if (!glb)
    goto error;

  // Add require function
  pth = natus_get_recursive_utf8(config, CFG_PATH);
  if (pth && natus_is_array(pth) &&
      natus_as_long(natus_get_utf8(pth, "length")) > 0) {
    // Setup the require function
    natusValue *require = natus_new_function(glb, require_js, "require");
    if (!natus_is_function(require) ||
        !natus_as_bool(natus_set_utf8(glb, "require", require, natusPropAttrConstant))) {
      natus_decref(require);
      goto error;
    }

    // Add the paths variable if we are not in a sandbox
    natusValue *whitelist = natus_get_utf8(config, CFG_WHITELIST);
    if (!natus_is_array(whitelist))
      natus_decref(natus_set_utf8(require, "paths", pth, natusPropAttrConstant));
    natus_decref(whitelist);

    natus_decref(require);
  }

  // Setup cache
  cache = natus_new_object(glb, NULL);
  if (!cache || !natus_set_private_name_value(glb, NATUS_REQUIRE_CACHE, cache)) {
    natus_decref(cache);
    goto error;
  }
  natus_decref(cache);

  // Finish initialization
  if (!natus_set_private_name_value(glb, NATUS_REQUIRE_CONFIG, config))
    goto error;

  natus_decref(pth);
  return true;

error:
  natus_decref(pth);
  natus_del_utf8(glb, "require");
  natus_set_private_name_value(glb, NATUS_REQUIRE_CACHE, NULL);
  natus_set_private_name_value(glb, NATUS_REQUIRE_CONFIG, NULL);
  return false;
}

natusValue *
natus_require_get_config(natusValue *ctx)
{
  return natus_get_private_name_value(natus_get_global(ctx),
                                      NATUS_REQUIRE_CONFIG);
}

bool
natus_require_hook_add(natusValue *ctx, const char *name, natusRequireHook func, void *misc, natusFreeFunction free)
{
  natusRequire *req = get_require(ctx);
  if (!req)
    return false;

  reqHook *tmp = mem_new(req, reqHook);
  if (!tmp || !mem_name_set(tmp, name)) {
    mem_free(tmp);
    return false;
  }
  mem_destructor_set(tmp, hook_dtor);

  tmp->misc = misc;
  tmp->free = free;
  tmp->func = func;
  tmp->next = req->hooks;
  req->hooks = tmp;
  return true;
}

bool
natus_require_hook_del(natusValue *ctx, const char *name)
{
  reqHook *tmp, **prv;
  natusRequire *req;

  req = get_require(ctx);
  if (!name || !req)
    return false;

  for (prv = &req->hooks, tmp = req->hooks; tmp; prv = &tmp->next, tmp = tmp->next) {
    if (!strcmp(mem_name_get(tmp), name)) {
      *prv = tmp->next;
      mem_decref(req, tmp);
      return true;
    }
  }

  return false;
}

bool
natus_require_origin_matcher_add(natusValue *ctx, const char *name, natusRequireOriginMatcher func, void *misc, natusFreeFunction free)
{
  natusRequire *req = get_require(ctx);
  if (!req)
    return false;

  reqOriginMatcher *tmp = mem_new(req, reqOriginMatcher);
  if (!tmp || !mem_name_set(tmp, name)) {
    mem_free(tmp);
    return false;
  }
  mem_destructor_set(tmp, originmatcher_dtor);

  tmp->misc = misc;
  tmp->free = free;
  tmp->func = func;
  tmp->next = req->matchers;
  req->matchers = tmp;
  return true;
}

bool
natus_require_origin_matcher_del(natusValue *ctx, const char *name)
{
  reqOriginMatcher *tmp, **prv;
  natusRequire *req;

  req = get_require(ctx);
  if (!name || !req)
    return false;

  for (prv = &req->matchers, tmp = req->matchers; tmp; prv = &tmp->next, tmp = tmp->next) {
    if (!strcmp(mem_name_get(tmp), name)) {
      *prv = tmp->next;
      mem_decref(req, tmp);
      return true;
    }
  }

  return false;
}

bool
natus_require_origin_permitted(natusValue *ctx, const char *uri)
{
  // Get the global
  natusValue *glb = natus_get_global(ctx);
  if (!glb)
    return true;

  // Get the require structure
  natusRequire *req = get_require(ctx);
  if (!req)
    return true;

  natusValue *config = natus_require_get_config(ctx);
  if (!config)
    return true;

  // Get the whitelist and blacklist
  natusValue *whitelist = natus_get_recursive_utf8(config, CFG_ORIGINS_WHITELIST);
  natusValue *blacklist = natus_get_recursive_utf8(config, CFG_ORIGINS_BLACKLIST);
  natus_decref(config);

  // Check to see if the URI is on the approved whitelist
  bool match = !natus_is_array(whitelist);
  reqOriginMatcher *tmp;
  int i;
  long len = natus_as_long(natus_get_utf8(whitelist, "length"));
  for (i=0; !match && i < len; i++) {
    char *pattern = natus_as_string_utf8(natus_get_index(whitelist, i), NULL);
    if (!pattern)
      continue;
    for (tmp = req->matchers; !match && tmp; tmp = tmp->next)
      match = tmp->func(pattern, uri);
    free(pattern);
  }
  natus_decref(whitelist);

  // If the URI matched the whitelist, check the blacklist
  if (match && natus_is_array(blacklist)) {
    len = natus_as_long(natus_get_utf8(blacklist, "length"));
    for (i = 0; match && i < len; i++) {
      char *pattern = natus_as_string_utf8(natus_get_index(blacklist, i), NULL);
      if (!pattern)
        continue;
      for (tmp = req->matchers; match && tmp; tmp = tmp->next)
        match = !tmp->func(pattern, uri);
      free(pattern);
    }
  }
  natus_decref(blacklist);

  return match;
}

typedef struct potential {
  natusRequireHook  hook;
  void             *misc;
  natusValue       *uris;
  struct potential *next;
} potential;

static potential *
get_potentials(reqHook *hook, natusValue *global, natusValue *name, natusValue **exc)
{
  natusValue *tmp = NULL;
  potential *pset = NULL, *p = NULL;

  for (; hook; hook = hook->next) {
    tmp = hook->func(global, natusRequireHookStepResolve, name, NULL, hook->misc);
    if (!tmp)
      continue;

    if (natus_is_exception(tmp)) {
      *exc = tmp;
      mem_free(pset);
      return NULL;
    }

    /* Handle invalid values by reverting to the provided name. */
    if (!natus_is_array(tmp)) {
      if (!natus_is_string(tmp)) {
        natus_decref(tmp);
        tmp = natus_incref(name);
      }

      natusValue *atmp = natus_new_array(global, tmp, NULL);
      natus_decref(tmp);
      tmp = atmp;
    }

    if (natus_as_long(natus_get_utf8(tmp, "length")) > 0 &&
        (p = mem_new(NULL, potential))) {
      assert(!pset || mem_steal(p, pset));
      assert(mem_steal(p, tmp));

      p->hook = hook->func;
      p->misc = hook->misc;
      p->uris = tmp;
      p->next = pset;
      pset = p;
    } else
      natus_decref(tmp);
  }

  *exc = NULL;
  return pset;
}

static natusValue *
check_cache(potential *pset, natusValue **uri)
{
  natusValue *module = NULL, *cache = NULL;
  potential *p;

  for (p=pset; p; p=p->next) {
    size_t i, len = natus_as_long(natus_get_utf8(p->uris, "length"));
    for (i=0; i < len; i++) {
      *uri = natus_get_index(p->uris, i);

      /* Don't check for errors here since we don't really care if there is
       * no cache yet... */
      if (!cache)
        cache = natus_get_private_name_value(natus_get_global(p->uris),
                                             NATUS_REQUIRE_CACHE);

      module = natus_get(cache, *uri);
      if (natus_is_object(module))
        return module;

      natus_decref(*uri);
      natus_decref(module);
    }
  }

  return NULL;
}

static natusValue *
load_module(potential *pset, natusValue *global,
            natusValue *name, natusValue **uri)
{
  natusValue *config = NULL, *whitelist = NULL, *module = NULL;
  potential *p;

  config = natus_get_private_name_value(global, NATUS_REQUIRE_CONFIG);
  whitelist = natus_get_recursive_utf8(config, CFG_WHITELIST);
  if (natus_is_exception(config)) {
    natus_decref(config);
    config = NULL;
  }
  if (natus_is_exception(whitelist)) {
    natus_decref(whitelist);
    whitelist = NULL;
  }

  for (p=pset; p; p=p->next) {
    size_t i, len = natus_as_long(natus_get_utf8(p->uris, "length"));
    for (i=0; i < len; i++) {
      *uri = natus_get_index(p->uris, i);
      module = p->hook(global, natusRequireHookStepLoad, name, *uri, p->misc);
      if (!natus_is_exception(module) && natus_is_object(module))
        goto havemod;
      natus_decref(*uri);
      *uri = NULL;
      if (module && natus_is_exception(module))
        goto out;
      natus_decref(module);
      module = NULL;
    }
  }

  goto out;

havemod:
  // Set the module id
  natus_decref(natus_set_recursive_utf8(module, "module.id", name,
                                        natusPropAttrConstant, true));

  // Set the module uri (if not sandboxed)
  if (natus_is_object(config) && !natus_is_array(whitelist))
    natus_decref(natus_set_recursive_utf8(module, "module.uri", *uri,
                                          natusPropAttrConstant, true));

  /* At this point we actually require the cache, so create it if it
   * doesn't exist yet... */
  natusValue *cache = natus_get_private_name_value(global, NATUS_REQUIRE_CACHE);
  if (natus_is_exception(cache) || !natus_is_object(cache)) {
    natus_decref(cache);
    cache = natus_new_object(global, NULL);
    if (natus_is_exception(cache) ||
        !natus_set_private_name_value(global, NATUS_REQUIRE_CACHE, cache)) {
      natus_decref(cache);
      goto out;
    }
  }

  // Store the module in the cache
  natus_decref(natus_set(cache, *uri, module, natusPropAttrNone));

out:
  natus_decref(config);
  natus_decref(whitelist);
  return module;
}

natusValue *
natus_require(natusValue *ctx, natusValue *name)
{
  natusValue *module = NULL, *uri = NULL;
  potential *pset = NULL;
  reqHook *tmp = NULL;

  natusValue *global = natus_get_global(ctx);
  if (!global)
    return NULL;

  natusRequire *req = get_require(ctx);
  if (!req)
    return NULL;

  // Check to see if we've already loaded the module (resolve step)
  pset = get_potentials(req->hooks, global, name, &module);
  if (module)
    goto out;
  if (pset) {
    module = check_cache(pset, &uri);
    if (module)
      goto out;

    // Load the module (load step)
    module = load_module(pset, global, name, &uri);
    if (module && natus_is_exception(module))
      goto out;
    else if (natus_is_object(module))
      goto modfound;
  }

  // Module not found
  natus_decref(module);
  char *modname = natus_to_string_utf8(name, NULL);
  module = natus_throw_exception(ctx, NULL, "RequireError", "Module %s not found!", modname);
  free(modname);
  goto out;

  // Module found
modfound:
  for (tmp = req->hooks; tmp; tmp = tmp->next) {
    natusValue *vtmp = tmp->func(module, natusRequireHookStepProcess,
                                 name, uri, tmp->misc);
    if (vtmp && natus_is_exception(vtmp)) {
      natus_decref(module);
      natus_decref(uri);
      module = vtmp;
      goto out;
    }

    natus_decref(vtmp);
  }

out:
  mem_free(pset);
  return module;
}

natusValue *
natus_require_utf8(natusValue *ctx, const char *name)
{
  natusValue *nm = natus_new_string_utf8(ctx, name);
  natusValue *tmp = natus_require(ctx, nm);
  natus_decref(nm);
  return tmp;
}

natusValue *
natus_require_utf16(natusValue *ctx, const natusChar *name)
{
  natusValue *nm = natus_new_string_utf16(ctx, name);
  natusValue *tmp = natus_require(ctx, nm);
  natus_decref(nm);
  return tmp;
}
