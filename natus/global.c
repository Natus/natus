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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
typedef void* natusEngCtx;
typedef void* natusEngVal;
#include <natus-internal.h>
#include <libmem.h>

#define  _str(s) # s
#define __str(s) _str(s)

static bool
do_load_file(const char *filename, bool reqsym,
             void **dll, natusEngineSpec **spec)
{
  /* Open the file */
  *dll = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!*dll) {
    fprintf(stderr, "%s -- %s\n", filename, dlerror());
    return false;
  }

  /* Get the engine spec */
  *spec = (natusEngineSpec*) dlsym(*dll, __str(NATUS_ENGINE_));
  if (!*spec || (*spec)->version != NATUS_ENGINE_VERSION) {
    dlclose(*dll);
    return false;
  }

  /* Do the symbol check */
  if ((*spec)->symbol && reqsym) {
    void *tmp = dlopen(NULL, 0);
    if (!tmp || !dlsym(tmp, (*spec)->symbol)) {
      if (tmp)
        dlclose(tmp);
      dlclose(*dll);
      return false;
    }
    dlclose(tmp);
  }

  return true;
}

static bool
do_load_dir(const char *dirname, bool reqsym,
            void **dll, natusEngineSpec **spec)
{
  bool res = false;
  DIR *dir = opendir(dirname);
  if (!dir)
    return res;

  struct dirent *ent = NULL;
  while ((ent = readdir(dir))) {
    size_t flen = strlen(ent->d_name);
    size_t slen = strlen(MODSUFFIX);

    if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name))
      continue;
    if (flen < slen || strcmp(ent->d_name + flen - slen, MODSUFFIX))
      continue;

    char *tmp = (char *) malloc(sizeof(char) * (flen + strlen(dirname) + 2));
    if (!tmp)
      continue;

    strcpy(tmp, dirname);
    strcat(tmp, "/");
    strcat(tmp, ent->d_name);

    if ((res = do_load_file(tmp, reqsym, dll, spec))) {
      free(tmp);
      break;
    }
    free(tmp);
  }

  closedir(dir);
  return res;
}

static void
dll_dtor(void **dll)
{
  if (!dll)
    return;
  mem_decref_children(dll);
  if (*dll)
    dlclose(*dll);
}

static void
value_dtor(natusValue *self)
{
  if (self->val && self->ctx && self->ctx->spec) {
    if (self->flag & natusEngValFlagUnlock)
      self->ctx->spec->val_unlock(self->ctx->ctx, self->val);
    if (self->flag & natusEngValFlagFree)
      self->ctx->spec->val_free(self->val);
  }
}

static void
context_dtor(natusContext *ctx)
{
  if (ctx && ctx->ctx && ctx->spec)
    ctx->spec->ctx_free(ctx->ctx);
}

static bool
ctx_get_dll(void *parent, void **child, void ***data)
{
  *data = child;
  return false;
}

natusValue *
mkval(const natusValue *ctx, natusEngVal val, natusEngValFlags flags, natusValueType type)
{
  if (!ctx || !val)
    return NULL;

  natusValue *self = mem_new_zero(NULL, natusValue);
  if (!self) {
    if (flags & natusEngValFlagUnlock)
      ctx->ctx->spec->val_unlock(ctx->ctx, val);
    if (flags & natusEngValFlagFree)
      ctx->ctx->spec->val_free(val);
    return NULL;
  }
  mem_destructor_set(self, value_dtor);

  self->type = flags & natusEngValFlagException ? natusValueTypeUnknown : type;
  self->flag = flags;
  self->val  = val;
  self->ctx  = mem_incref(self, ctx->ctx);
  if (!self->ctx) {
    mem_free(self);
    return NULL;
  }

  return self;
}

natusValue *
natus_incref(natusValue *val)
{
  return mem_incref(NULL, val);
}

void
natus_decref(natusValue *val)
{
  mem_decref(NULL, val);
}

natusValue *
natus_new_global(const char *name_or_path)
{
  natusPrivate *priv = NULL;
  natusValue *self = NULL;
  void **dll = NULL;
  bool res = false;

  self = mem_new_zero(NULL, natusValue);
  if (!self)
    goto error;
  mem_destructor_set(self, value_dtor);

  self->ctx = mem_new_zero(self, natusContext);
  if (!self->ctx)
    goto error;
  mem_destructor_set(self->ctx, context_dtor);

  dll = mem_new_zero(self->ctx, void*);
  if (!dll || !mem_name_set(dll, "dll"))
    goto error;
  mem_destructor_set(dll, dll_dtor);

  priv = mem_new_size(dll, 0);
  if (!priv)
    goto error;

  /* Load the engine */
  if (name_or_path) {
    res = do_load_file(name_or_path, false, dll, &self->ctx->spec);
    if (!res) {
      char *tmp = NULL;
      if (asprintf(&tmp, "%s/%s%s", __str(ENGINEDIR), name_or_path, MODSUFFIX) >= 0) {
        res = do_load_file(tmp, false, dll, &self->ctx->spec);
        free(tmp);
      }
    }
  } else {
      res = do_load_dir(__str(ENGINEDIR), true, dll, &self->ctx->spec);
      if (!res)
        res = do_load_dir(__str(ENGINEDIR), false, dll, &self->ctx->spec);
  }
  if (!res)
    goto error;

  if (!private_set(priv, NATUS_PRIV_GLOBAL, self, NULL))
    goto error;

  self->flag = natusEngValFlagUnlock | natusEngValFlagFree;
  if (!(self->val = self->ctx->spec->new_global(NULL, NULL, priv, &self->ctx->ctx, &self->flag)))
    goto error;

  return self;

error:
  mem_free(self);
  return NULL;
}

natusValue *
natus_new_global_shared(const natusValue *global)
{
  natusValue *self = NULL;
  natusEngCtx ctx = NULL;
  natusEngVal val = NULL;
  natusEngValFlags flags = natusEngValFlagUnlock | natusEngValFlagFree;
  void **dll = NULL;

  if (!global)
    return NULL;

  mem_children_foreach(global->ctx, "dll", ctx_get_dll, &dll);
  if (!dll)
    return NULL;

  natusPrivate *priv = mem_new_size(dll, 0);
  if (!priv)
    return NULL;

  val = global->ctx->spec->new_global(global->ctx->ctx, global->val, priv, &ctx, &flags);
  self = mkval(global, val, flags, natusValueTypeObject);
  if (!self)
    goto error;

  /* If a new context was created, wrap it */
  if (ctx && global->ctx->ctx != ctx) {
    mem_decref(self, global->ctx);

    self->ctx = mem_new_zero(self, natusContext);
    if (!self->ctx) {
      mem_free(priv);
      mem_free(self);
      global->ctx->spec->ctx_free(ctx);
      return NULL;
    }

    mem_destructor_set(self->ctx, context_dtor);
    mem_group(global->ctx, self->ctx);
    self->ctx->spec = global->ctx->spec;
    self->ctx->ctx  = ctx;

    if (!mem_incref(self->ctx, dll))
      goto error;
  }

  if (!private_set(priv, NATUS_PRIV_GLOBAL, self, NULL))
    goto error;

  return self;

error:
  mem_free(priv);
  mem_free(self);
  return NULL;
}

natusValue *
natus_get_global(const natusValue *ctx)
{
  if (!ctx)
    return NULL;

  natusValue *global = natus_get_private_name(ctx, NATUS_PRIV_GLOBAL);
  if (global)
    return global;

  natusEngValFlags flags = natusEngValFlagUnlock | natusEngValFlagFree;
  natusEngVal glb = ctx->ctx->spec->get_global(ctx->ctx->ctx, ctx->val, &flags);
  if (!glb)
    return NULL;

  natusPrivate *priv = ctx->ctx->spec->get_private(ctx->ctx->ctx, glb);
  if (flags & natusEngValFlagUnlock)
    ctx->ctx->spec->val_unlock(ctx->ctx->ctx, glb);
  if (flags & natusEngValFlagFree)
    ctx->ctx->spec->val_free(glb);
  if (!priv)
    return NULL;

  return private_get(priv, NATUS_PRIV_GLOBAL);
}

const char *
natus_get_engine_name(const natusValue *ctx)
{
  if (!ctx)
    return NULL;
  return ctx->ctx->spec->name;
}

bool
natus_borrow_context(const natusValue *ctx, void **context, void **value)
{
  if (!ctx)
    return false;
  return ctx->ctx->spec->borrow_context(ctx->ctx->ctx, ctx->val, context, value);
}
