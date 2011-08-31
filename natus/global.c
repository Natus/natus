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
#include <natus-engine.h>
#include <natus-internal.h>

#define  _str(s) # s
#define __str(s) _str(s)

struct privWrap {
  privWrap *next;
  privWrap *prev;
  privWrap **head;
  natusPrivate *priv;
};

static engine *engines;

static void
engine_decref(engine **head, engine *eng)
{
  if (!eng)
    return;
  if (eng->ref > 0)
    eng->ref--;
  if (eng->ref == 0) {
    dlclose(eng->dll);
    if (eng->prev)
      eng->prev->next = eng->next;
    else
      *head = eng->next;
    free(eng);
  }
}

static engine *
do_load_file(const char *filename, bool reqsym)
{
  engine *eng;
  void *dll;

  /* Open the file */
  dll = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!dll) {
    fprintf(stderr, "%s -- %s\n", filename, dlerror());
    return NULL;
  }

  /* See if this engine was already loaded */
  for (eng = engines; eng; eng = eng->next) {
    if (dll == eng->dll) {
      dlclose(dll);
      eng->ref++;
      return eng;
    }
  }

  /* Create a new engine */
  eng = new0(engine);
  if (!eng) {
    dlclose(dll);
    return NULL;
  }

  /* Get the engine spec */
  eng->spec = (natusEngineSpec*) dlsym(dll, __str(NATUS_ENGINE_));
  if (!eng->spec || eng->spec->version != NATUS_ENGINE_VERSION) {
    dlclose(dll);
    free(eng);
    return NULL;
  }

  /* Do the symbol check */
  if (eng->spec->symbol && reqsym) {
    void *tmp = dlopen(NULL, 0);
    if (!tmp || !dlsym(tmp, eng->spec->symbol)) {
      if (tmp)
        dlclose(tmp);
      dlclose(dll);
      free(eng);
      return NULL;
    }
    dlclose(tmp);
  }

  /* Insert the engine */
  eng->ref++;
  eng->dll = dll;
  eng->next = engines;
  if (eng->next)
    eng->next->prev = eng;
  return engines = eng;
}

static engine *
do_load_dir(const char *dirname, bool reqsym)
{
  engine *res = NULL;
  DIR *dir = opendir(dirname);
  if (!dir)
    return NULL;

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

    if ((res = do_load_file(tmp, reqsym))) {
      free(tmp);
      break;
    }
    free(tmp);
  }

  closedir(dir);
  return res;
}

static void
onprivfree(privWrap *pw)
{
  if (pw->prev)
    pw->prev->next = pw->next;
  else if (pw->head)
    *(pw->head) = pw->next;
  if (pw->next)
    pw->next->prev = pw->prev;
  free(pw);
}

static natusPrivate *
pushpriv(engCtx *ctx, privWrap *pw)
{
  if (!ctx || !pw)
    return NULL;

  pw->head = &ctx->priv;
  pw->next = ctx->priv;
  if (pw->next)
    pw->next->prev = pw;
  ctx->priv = pw;
  return pw->priv;
}

natusPrivate *
mkpriv(engCtx *ctx)
{
  privWrap *pw = new0(privWrap);
  if (!pw)
    return NULL;

  pw->priv = private_init(pw, (natusFreeFunction) onprivfree);
  if (!pw->priv) {
    free(pw);
    return NULL;
  }

  return pushpriv(ctx, pw);
}

natusValue *
natus_incref(natusValue *val)
{
  if (!val)
    return NULL;
  val->ref++;
  return val;
}

static bool
free_all_ctx(engCtx *ctx)
{
  engCtx *tmp;

  if (!ctx)
    return false;

  for (tmp = ctx; tmp; tmp = tmp->prev)
    if (tmp->ref > 0)
      return false;
  for (tmp = ctx->next; tmp; tmp = tmp->next)
    if (tmp->ref > 0)
      return false;

  return true;
}

static void
context_decref(engCtx *ctx)
{
  if (!ctx)
    return;
  /* Decrement the context reference */
  ctx->ref = ctx->ref > 0 ? ctx->ref - 1 : 0;

  /* If there are no more context references,
   * free it if there are no other related contexts... */
  if (free_all_ctx(ctx)) {
    engCtx *tmp, *head;

    head = ctx;
    while (head->prev)
      head = head->prev;

    /* First loop: free all the contexts */
    for (tmp = head; tmp; tmp = tmp->next)
      tmp->eng->spec->ctx_free(tmp->ctx);

    /* Second loop: free everything else */
    for (tmp = head; tmp;) {
      privWrap *ptmp = tmp->priv;
      while (ptmp) {
        privWrap *pw = ptmp->next;
        natus_private_free(ptmp->priv);
        ptmp = pw;
      }

      engine_decref(&engines, tmp->eng);

      engCtx *ntmp = tmp->next;
      free(tmp);
      tmp = ntmp;
    }

  }
}

void
natus_decref(natusValue *val)
{
  if (!val)
    return;
  val->ref = val->ref > 0 ? val->ref - 1 : 0;
  if (val->ref == 0) {
    /* Free the value */
    if (val->val && (val->flg & natusEngValFlagMustFree)) {
      val->ctx->eng->spec->val_unlock(val->ctx->ctx, val->val);
      val->ctx->eng->spec->val_free(val->val);
    }
    context_decref(val->ctx);
    free(val);
  }
}

natusValue *
natus_new_global(const char *name_or_path)
{
  natusValue *self = NULL;
  natusPrivate *priv = NULL;

  /* Create the value */
  self = new0(natusValue);
  if (!self)
    goto error;
  self->ref++;
  self->ctx = new0(engCtx);
  if (!self->ctx)
    goto error;
  self->ctx->ref++;

  /* Load the engine */
  if (name_or_path) {
    self->ctx->eng = do_load_file(name_or_path, false);
    if (!self->ctx->eng) {
      char *tmp = NULL;
      if (asprintf(&tmp, "%s/%s%s", __str(ENGINEDIR), name_or_path, MODSUFFIX) >= 0) {
        self->ctx->eng = do_load_file(tmp, false);
        free(tmp);
      }
    }
  } else {
    self->ctx->eng = engines;
    if (!self->ctx->eng) {
      self->ctx->eng = do_load_dir(__str(ENGINEDIR), true);
      if (!self->ctx->eng)
        self->ctx->eng = do_load_dir(__str(ENGINEDIR), false);
    }
  }
  if (!self->ctx->eng)
    goto error;

  priv = mkpriv(self->ctx);
  if (!priv || !private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
    natus_private_free(priv);
    goto error;
  }

  self->flg = natusEngValFlagMustFree;
  if (!(self->val = self->ctx->eng->spec->new_global(NULL, NULL, priv, &self->ctx->ctx, &self->flg)))
    goto error;

  return self;

error:
  if (self) {
    if (self->ctx) {
      if (self->ctx->eng)
        engine_decref(&engines, self->ctx->eng);
      free(self->ctx);
    }
    free(self);
  }
  return NULL;
}

natusValue *
natus_new_global_shared(const natusValue *global)
{
  natusValue *self = NULL;
  natusEngCtx ctx = NULL;
  natusEngVal val = NULL;
  natusEngValFlags flags = natusEngValFlagMustFree;

  privWrap *pw = new0(privWrap);
  if (!pw)
    return NULL;

  natusPrivate *priv = private_init(pw, (natusFreeFunction) onprivfree);
  if (!priv)
    return NULL;

  val = global->ctx->eng->spec->new_global(global->ctx->ctx, global->val, priv, &ctx, &flags);
  self = mkval(global, val, flags, natusValueTypeObject);
  if (!self)
    return NULL;

  /* If a new context was created, wrap it */
  if (global->ctx->ctx != ctx) {
    self->ctx->ref--;
    self->ctx = new0(engCtx);
    if (!self->ctx) {
      global->ctx->eng->spec->val_unlock(ctx, self->val);
      global->ctx->eng->spec->val_free(self->val);
      global->ctx->eng->spec->ctx_free(ctx);
      free(self);
      return NULL;
    }
    self->ctx->ref++;
    self->ctx->ctx = ctx;
    self->ctx->eng = global->ctx->eng;
    self->ctx->eng->ref++;
    self->ctx->prev = global->ctx;
    self->ctx->next = global->ctx->next;
    global->ctx->next = self->ctx;
  }

  if (!private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
    natus_decref(self);
    return NULL;
  }

  pushpriv(self->ctx, pw);
  return self;
}

natusValue *
natus_get_global(const natusValue *ctx)
{
  if (!ctx)
    return NULL;

  natusValue *global = natus_get_private_name(ctx, NATUS_PRIV_GLOBAL);
  if (global)
    return global;

  natusEngVal glb = ctx->ctx->eng->spec->get_global(ctx->ctx->ctx, ctx->val);
  if (!glb)
    return NULL;

  natusPrivate *priv = ctx->ctx->eng->spec->get_private(ctx->ctx->ctx, glb);
  ctx->ctx->eng->spec->val_unlock(ctx->ctx->ctx, glb);
  ctx->ctx->eng->spec->val_free(glb);
  if (!priv)
    return NULL;

  return private_get(priv, NATUS_PRIV_GLOBAL);
}

const char *
natus_get_engine_name(const natusValue *ctx)
{
  if (!ctx)
    return NULL;
  return ctx->ctx->eng->spec->name;
}

bool
natus_borrow_context(const natusValue *ctx, void **context, void **value)
{
  if (!ctx)
    return false;
  return ctx->ctx->eng->spec->borrow_context(ctx->ctx->ctx, ctx->val, context, value);
}
