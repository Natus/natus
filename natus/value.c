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

#define NATUS_PRIV_CLASS     "natus::Class"
#define NATUS_PRIV_FUNCTION  "natus::Function"
#define NATUS_PRIV_GLOBAL    "natus::Global"

#define callandmkval(n, t, c, f, ...) \
  natusEngValFlags _flags = natusEngValFlagMustFree; \
  natusEngVal _val = c->ctx->eng->spec->f(__VA_ARGS__, &_flags); \
  n = mkval(c, _val, _flags, t);

#define callandreturn(t, c, f, ...) \
  callandmkval(natusValue *_value, t, c, f, __VA_ARGS__); \
  return _value

typedef struct engine engine;
struct engine {
  size_t ref;
  engine *next;
  engine *prev;
  void *dll;
  natusEngineSpec *spec;
};

typedef struct privWrap privWrap;
struct privWrap {
  privWrap *next;
  privWrap *prev;
  privWrap **head;
  natusPrivate *priv;
};

typedef struct engCtx engCtx;
struct engCtx {
  size_t ref;
  engine *eng;
  natusEngCtx ctx;
  engCtx *next;
  engCtx *prev;
  privWrap *priv;
};

struct natusValue {
  size_t ref;
  engCtx *ctx;
  natusValueType typ;
  natusEngVal val;
  natusEngValFlags flg;
};

static engine *engines;

#define new0(type) ((type*) malloc0(sizeof(type)))
static void *
malloc0(size_t size)
{
  void *tmp = malloc(size);
  if (tmp)
    return memset(tmp, 0, size);
  return NULL;
}

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

static natusPrivate *
mkpriv(engCtx *ctx)
{
  privWrap *pw = new0(privWrap);
  if (!pw)
    return NULL;

  pw->priv = natus_private_init(pw, (natusFreeFunction) onprivfree);
  if (!pw->priv) {
    free(pw);
    return NULL;
  }

  return pushpriv(ctx, pw);
}

static natusValue *
mkval(const natusValue *ctx, natusEngVal val, natusEngValFlags flags, natusValueType type)
{
  if (!ctx || !val)
    return NULL;

  natusValue *self = new0(natusValue);
  if (!self) {
    ctx->ctx->eng->spec->val_unlock(ctx->ctx->ctx, val);
    ctx->ctx->eng->spec->val_free(val);
    return NULL;
  }

  self->ref++;
  self->ctx = ctx->ctx;
  self->ctx->ref++;
  self->val = val;
  self->flg = flags;
  self->typ = flags & natusEngValFlagException ? natusValueTypeUnknown : type;
  return self;
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
  if (!priv || !natus_private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
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

  natusPrivate *priv = natus_private_init(pw, (natusFreeFunction) onprivfree);
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

  if (!natus_private_set(priv, NATUS_PRIV_GLOBAL, self, NULL)) {
    natus_decref(self);
    return NULL;
  }

  pushpriv(self->ctx, pw);
  return self;
}

natusValue *
natus_new_boolean(const natusValue *ctx, bool b)
{
  if (!ctx)
    return NULL;
  callandreturn(natusValueTypeBoolean, ctx, new_bool, ctx->ctx->ctx, b);
}

natusValue *
natus_new_number(const natusValue *ctx, double n)
{
  if (!ctx)
    return NULL;
  callandreturn(natusValueTypeNumber, ctx, new_number, ctx->ctx->ctx, n);
}

natusValue *
natus_new_string(const natusValue *ctx, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  natusValue *tmpv = natus_new_string_varg(ctx, fmt, ap);
  va_end(ap);
  return tmpv;
}

natusValue *
natus_new_string_varg(const natusValue *ctx, const char *fmt, va_list arg)
{
  char *tmp = NULL;
  if (vasprintf(&tmp, fmt, arg) < 0)
    return NULL;
  natusValue *tmpv = natus_new_string_utf8(ctx, tmp);
  free(tmp);
  return tmpv;
}

natusValue *
natus_new_string_utf8(const natusValue *ctx, const char *string)
{
  if (!string)
    return NULL;
  return natus_new_string_utf8_length(ctx, string, strlen(string));
}

natusValue *
natus_new_string_utf8_length(const natusValue *ctx, const char *string, size_t len)
{
  if (!ctx || !string)
    return NULL;
  callandreturn(natusValueTypeString, ctx, new_string_utf8, ctx->ctx->ctx, string, len);
}

natusValue *
natus_new_string_utf16(const natusValue *ctx, const natusChar *string)
{
  size_t len;
  for (len = 0; string && string[len] != L'\0'; len++)
    ;
  return natus_new_string_utf16_length(ctx, string, len);
}

natusValue *
natus_new_string_utf16_length(const natusValue *ctx, const natusChar *string, size_t len)
{
  if (!ctx || !string)
    return NULL;
  callandreturn(natusValueTypeString, ctx, new_string_utf16, ctx->ctx->ctx, string, len);
}

natusValue *
natus_new_array(const natusValue *ctx, ...) {
  va_list ap;

  va_start(ap, ctx);
  natusValue *ret = natus_new_array_varg(ctx, ap);
  va_end(ap);

  return ret;
}

natusValue *
natus_new_array_vector(const natusValue *ctx, const natusValue **array)
{
  const natusValue *novals[1] =
    { NULL, };
  size_t count = 0;
  size_t i;

  if (!ctx)
    return NULL;
  if (!array)
    array = novals;

  for (count = 0; array[count]; count++)
    ;

  natusEngVal *vals = (natusEngVal*) malloc(sizeof(natusEngVal) * (count + 1));
  if (!vals)
    return NULL;

  for (i = 0; i < count; i++)
    vals[i] = array[i]->val;

  callandmkval(natusValue *val, natusValueTypeUnknown, ctx, new_array, ctx->ctx->ctx, vals, count);
  free(vals);
  return val;
}

natusValue *
natus_new_array_varg(const natusValue *ctx, va_list ap)
{
  va_list apc;
  size_t count = 0, i = 0;

  va_copy(apc, ap);
  while (va_arg(apc, natusValue*))
    count++;
  va_end(apc);

  const natusValue **array = NULL;
  if (count > 0) {
    array = calloc(++count, sizeof(natusValue*));
    if (!array)
      return NULL;
    va_copy(apc, ap);
    while (--count > 0)
      array[i++] = va_arg(apc, natusValue*);
    array[i] = NULL;
    va_end(apc);
  }

  natusValue *ret = natus_new_array_vector(ctx, array);
  free(array);
  return ret;
}

natusValue *
natus_new_function(const natusValue *ctx, natusNativeFunction func, const char *name)
{
  if (!ctx || !func)
    return NULL;

  natusPrivate *priv = mkpriv(ctx->ctx);
  if (!priv)
    goto error;
  if (!natus_private_set(priv, NATUS_PRIV_GLOBAL, natus_get_global(ctx), NULL))
    goto error;
  if (!natus_private_set(priv, NATUS_PRIV_FUNCTION, func, NULL))
    goto error;

  callandreturn(natusValueTypeFunction, ctx, new_function, ctx->ctx->ctx, name, priv);

error:
  natus_private_free(priv);
  return NULL;
}

natusValue *
natus_new_object(const natusValue *ctx, natusClass *cls)
{
  if (!ctx)
    return NULL;

  natusPrivate *priv = mkpriv(ctx->ctx);
  if (!priv)
    goto error;
  if (cls && !natus_private_set(priv, NATUS_PRIV_GLOBAL, natus_get_global(ctx), NULL))
    goto error;
  if (cls && !natus_private_set(priv, NATUS_PRIV_CLASS, cls, (natusFreeFunction) cls->free))
    goto error;

  callandreturn(natusValueTypeObject, ctx, new_object, ctx->ctx->ctx, cls, priv);

error:
  natus_private_free(priv);
  return NULL;
}

natusValue *
natus_new_null(const natusValue *ctx)
{
  if (!ctx)
    return NULL;

  callandreturn(natusValueTypeNull, ctx, new_null, ctx->ctx->ctx);
}

natusValue *
natus_new_undefined(const natusValue *ctx)
{
  if (!ctx)
    return NULL;
  callandreturn(natusValueTypeUndefined, ctx, new_undefined, ctx->ctx->ctx);
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

  return natus_private_get(priv, NATUS_PRIV_GLOBAL);
}

const char *
natus_get_engine_name(const natusValue *ctx)
{
  if (!ctx)
    return NULL;
  return ctx->ctx->eng->spec->name;
}

natusValueType
natus_get_type(const natusValue *ctx)
{
  if (!ctx)
    return natusValueTypeUndefined;
  if (ctx->typ == natusValueTypeUnknown)
    ((natusValue*) ctx)->typ = ctx->ctx->eng->spec->get_type(ctx->ctx->ctx, ctx->val);
  return ctx->typ;
}

const char *
natus_get_type_name(const natusValue *ctx)
{
  switch (natus_get_type(ctx)) {
  case natusValueTypeArray:
    return "array";
  case natusValueTypeBoolean:
    return "boolean";
  case natusValueTypeFunction:
    return "function";
  case natusValueTypeNull:
    return "null";
  case natusValueTypeNumber:
    return "number";
  case natusValueTypeObject:
    return "object";
  case natusValueTypeString:
    return "string";
  case natusValueTypeUndefined:
    return "undefined";
  default:
    return "unknown";
  }
}

bool
natus_borrow_context(const natusValue *ctx, void **context, void **value)
{
  if (!ctx)
    return false;
  return ctx->ctx->eng->spec->borrow_context(ctx->ctx->ctx, ctx->val, context, value);
}

bool
natus_is_exception(const natusValue *val)
{
  return !val || (val->flg & natusEngValFlagException);
}

bool
natus_is_type(const natusValue *val, natusValueType types)
{
  return natus_get_type(val) & types;
}

bool
natus_is_array(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeArray);
}

bool
natus_is_boolean(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeBoolean);
}

bool
natus_is_function(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeFunction);
}

bool
natus_is_null(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeNull);
}

bool
natus_is_number(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeNumber);
}

bool
natus_is_object(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeObject);
}

bool
natus_is_string(const natusValue *val)
{
  return natus_is_type(val, natusValueTypeString);
}

bool
natus_is_undefined(const natusValue *val)
{
  return !val || natus_is_type(val, natusValueTypeUndefined);
}

bool
natus_to_bool(const natusValue *val)
{
  if (!val)
    return false;
  if (natus_is_exception(val))
    return false;

  return val->ctx->eng->spec->to_bool(val->ctx->ctx, val->val);
}

double
natus_to_double(const natusValue *val)
{
  if (!val)
    return 0;
  return val->ctx->eng->spec->to_double(val->ctx->ctx, val->val);
}

natusValue *
natus_to_exception(natusValue *val)
{
  if (!val)
    return NULL;
  val->flg |= natusEngValFlagException;
  return val;
}

int
natus_to_int(const natusValue *val)
{
  if (!val)
    return 0;
  return (int) natus_to_double(val);
}

long
natus_to_long(const natusValue *val)
{
  if (!val)
    return 0;
  return (long) natus_to_double(val);
}

char *
natus_to_string_utf8(const natusValue *val, size_t *len)
{
  if (!val)
    return 0;
  size_t intlen = 0;
  if (!len)
    len = &intlen;

  if (!natus_is_string(val)) {
    natusValue *str = natus_call_utf8_array((natusValue*) val, "toString", NULL);
    if (natus_is_string(str)) {
      char *tmp = val->ctx->eng->spec->to_string_utf8(str->ctx->ctx, str->val, len);
      natus_decref(str);
      return tmp;
    }
    natus_decref(str);
  }

  return val->ctx->eng->spec->to_string_utf8(val->ctx->ctx, val->val, len);
}

natusChar *
natus_to_string_utf16(const natusValue *val, size_t *len)
{
  if (!val)
    return 0;
  size_t intlen = 0;
  if (!len)
    len = &intlen;

  if (!natus_is_string(val)) {
    natusValue *str = natus_call_utf8_array((natusValue*) val, "toString", NULL);
    if (natus_is_string(str)) {
      natusChar *tmp = val->ctx->eng->spec->to_string_utf16(str->ctx->ctx, str->val, len);
      natus_decref(str);
      return tmp;
    }
    natus_decref(str);
  }

  return val->ctx->eng->spec->to_string_utf16(val->ctx->ctx, val->val, len);
}

bool
natus_as_bool(natusValue *val)
{
  bool b = natus_to_bool(val);
  natus_decref(val);
  return b;
}

double
natus_as_double(natusValue *val)
{
  double d = natus_to_double(val);
  natus_decref(val);
  return d;
}

int
natus_as_int(natusValue *val)
{
  int i = natus_to_int(val);
  natus_decref(val);
  return i;
}

long
natus_as_long(natusValue *val)
{
  long l = natus_to_long(val);
  natus_decref(val);
  return l;
}

char *
natus_as_string_utf8(natusValue *val, size_t *len)
{
  char *s = natus_to_string_utf8(val, len);
  natus_decref(val);
  return s;
}

natusChar *
natus_as_string_utf16(natusValue *val, size_t *len)
{
  natusChar *s = natus_to_string_utf16(val, len);
  natus_decref(val);
  return s;
}

natusValue *
natus_del(natusValue *val, const natusValue *id)
{
  if (!natus_is_type(id, natusValueTypeNumber | natusValueTypeString))
    return NULL;

  // If the object is native, skip argument conversion and js overhead;
  //    call directly for increased speed
  natusClass *cls = natus_get_private_name(val, NATUS_PRIV_CLASS);
  if (cls && cls->del) {
    natusValue *rslt = cls->del(cls, val, id);
    if (!natus_is_undefined(rslt) || !natus_is_exception(rslt))
      return rslt;
    natus_decref(rslt);
  }

  callandreturn(natusValueTypeUnknown, val, del, val->ctx->ctx, val->val, id->val);
}

natusValue *
natus_del_utf8(natusValue *val, const char *id)
{
  natusValue *vid = natus_new_string_utf8(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_del(val, vid);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_del_index(natusValue *val, size_t id)
{
  natusValue *vid = natus_new_number(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_del(val, vid);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_del_recursive_utf8(natusValue *obj, const char *id)
{
  if (!obj || !id)
    return NULL;
  if (!strcmp(id, ""))
    return obj;

  char *next = strchr(id, '.');
  if (next == NULL
    )
    return natus_del_utf8(obj, id);
  char *base = strdup(id);
  if (!base)
    return NULL;
  base[next++ - id] = '\0';

  natusValue *basev = natus_get_utf8(obj, base);
  natusValue *tmp = natus_del_recursive_utf8(basev, next);
  natus_decref(basev);
  free(base);
  return tmp;
}

natusValue *
natus_get(natusValue *val, const natusValue *id)
{
  if (!natus_is_type(id, natusValueTypeNumber | natusValueTypeString))
    return NULL;

  // If the object is native, skip argument conversion and js overhead;
  //    call directly for increased speed
  natusClass *cls = natus_get_private_name(val, NATUS_PRIV_CLASS);
  if (cls && cls->get) {
    natusValue *rslt = cls->get(cls, val, id);
    if (!natus_is_undefined(rslt) || !natus_is_exception(rslt))
      return rslt;
    natus_decref(rslt);
  }

  callandreturn(natusValueTypeUnknown, val, get, val->ctx->ctx, val->val, id->val);
}

natusValue *
natus_get_utf8(natusValue *val, const char *id)
{
  natusValue *vid = natus_new_string_utf8(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_get(val, vid);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_get_index(natusValue *val, size_t id)
{
  natusValue *vid = natus_new_number(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_get(val, vid);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_get_recursive_utf8(natusValue *obj, const char *id)
{
  if (!obj || !id)
    return NULL;
  if (!strcmp(id, ""))
    return obj;

  char *next = strchr(id, '.');
  if (next == NULL
    )
    return natus_get_utf8(obj, id);
  char *base = strdup(id);
  if (!base)
    return NULL;
  base[next++ - id] = '\0';

  natusValue *basev = natus_get_utf8(obj, base);
  natusValue *tmp = natus_get_recursive_utf8(basev, next);
  natus_decref(basev);
  free(base);
  return tmp;
}

natusValue *
natus_set(natusValue *val, const natusValue *id, const natusValue *value, natusPropAttr attrs)
{
  if (!natus_is_type(id, natusValueTypeNumber | natusValueTypeString))
    return NULL;
  if (!value)
    return NULL;

  // If the object is native, skip argument conversion and js overhead;
  //    call directly for increased speed
  natusClass *cls = natus_get_private_name(val, NATUS_PRIV_CLASS);
  if (cls && cls->set) {
    natusValue *rslt = cls->set(cls, val, id, value);
    if (!natus_is_undefined(rslt) || !natus_is_exception(rslt))
      return rslt;
    natus_decref(rslt);
  }

  callandreturn(natusValueTypeUnknown, val, set, val->ctx->ctx, val->val, id->val, value->val, attrs);
}

natusValue *
natus_set_utf8(natusValue *val, const char *id, const natusValue *value, natusPropAttr attrs)
{
  natusValue *vid = natus_new_string_utf8(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_set(val, vid, value, attrs);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_set_index(natusValue *val, size_t id, const natusValue *value)
{
  natusValue *vid = natus_new_number(val, id);
  if (!vid)
    return NULL;
  natusValue *ret = natus_set(val, vid, value, natusPropAttrNone);
  natus_decref(vid);
  return ret;
}

natusValue *
natus_set_recursive_utf8(natusValue *obj, const char *id, const natusValue *value, natusPropAttr attrs, bool mkpath)
{
  if (!obj || !id)
    return NULL;
  if (!strcmp(id, ""))
    return obj;

  char *next = strchr(id, '.');
  if (next == NULL
    )
    return natus_set_utf8(obj, id, value, attrs);
  char *base = strdup(id);
  if (!base)
    return NULL;
  base[next++ - id] = '\0';

  natusValue *step = natus_get_utf8(obj, base);
  if (mkpath && (!step || natus_is_undefined(step))) {
    natus_decref(step);
    step = natus_new_object(obj, NULL);
    if (step) {
      natusValue *rslt = natus_set_utf8(obj, base, step, attrs);
      if (natus_is_exception(rslt)) {
        natus_decref(step);
        free(base);
        return rslt;
      }
      natus_decref(rslt);
    }
  }
  free(base);

  natusValue *tmp = natus_set_recursive_utf8(step, next, value, attrs, mkpath);
  natus_decref(step);
  return tmp;
}

natusValue *
natus_enumerate(natusValue *val)
{
  // If the object is native, skip argument conversion and js overhead;
  //    call directly for increased speed
  natusClass *cls = natus_get_private_name(val, NATUS_PRIV_CLASS);
  if (cls && cls->enumerate)
    return cls->enumerate(cls, val);

  callandreturn(natusValueTypeArray, val, enumerate, val->ctx->ctx, val->val);
}

bool
natus_set_private_name(natusValue *obj, const char *key, void *priv, natusFreeFunction free)
{
  if (!natus_is_type(obj, natusValueTypeSupportsPrivate))
    return false;
  if (!key)
    return false;

  natusPrivate *prv = obj->ctx->eng->spec->get_private(obj->ctx->ctx, obj->val);
  return natus_private_set(prv, key, priv, free);
}

static void
free_private_value(natusValue *pv)
{
  if (!pv)
    return;

  /* If the refcount is 0 it means that we are in the process of teardown,
   * and the value has already been unlocked because we are dismantling ctx.
   * Thus, we only do unlock if we aren't in this teardown phase. */
  if (pv->ctx->ref > 0)
    pv->ctx->eng->spec->val_unlock(pv->ctx->ctx, pv->val);
  pv->ctx->eng->spec->val_free(pv->val);
  free(pv);
}

bool
natus_set_private_name_value(natusValue *obj, const char *key, natusValue* priv)
{
  natusEngVal v = NULL;
  natusValue *val = NULL;

  if (priv) {
    /* NOTE: So we have this circular problem where if we hide just a normal
     *       natusValue* it keeps a reference to the ctx, but the hidden value
     *       won't be free'd until the ctx is destroyed, but that won't happen
     *       because we have a reference.
     *
     *       So the way around this is that we don't store the normal natusValue,
     *       but instead store a special one without a refcount on the ctx.
     *       This means that ctx will be properly freed. */
    v = priv->ctx->eng->spec->val_duplicate(priv->ctx->ctx, priv->val);
    if (!v)
      return false;

    val = mkval(priv, v, priv->flg, priv->typ);
    if (!val)
      return false;
    val->ctx->ref--; /* Don't keep a copy of this reference around */
  }

  if (!natus_set_private_name(obj, key, val, (natusFreeFunction) free_private_value)) {
    free_private_value(val);
    return false;
  }
  return true;
}

void*
natus_get_private_name(const natusValue *obj, const char *key)
{
  if (!natus_is_type(obj, natusValueTypeSupportsPrivate))
    return NULL;
  if (!key)
    return NULL;
  natusPrivate *prv = obj->ctx->eng->spec->get_private(obj->ctx->ctx, obj->val);
  return natus_private_get(prv, key);
}

natusValue *
natus_get_private_name_value(const natusValue *obj, const char *key)
{
  /* See note in natus_set_private_name_value() */
  natusValue *val = natus_get_private_name(obj, key);
  if (!val)
    return NULL;
  return mkval(val,
      val->ctx->eng->spec->val_duplicate(val->ctx->ctx, val->val),
      val->flg,
      val->typ);
}

natusValue *
natus_evaluate(natusValue *ths, const natusValue *javascript, const natusValue *filename, unsigned int lineno)
{
  if (!ths || !javascript)
    return NULL;
  natusValue *jscript = NULL;

  // Remove shebang line if present
  size_t len;
  natusChar *chars = natus_to_string_utf16(javascript, &len);
  if (len > 2 && chars[0] == '#' && chars[1] == '!') {
    size_t i;
    for (i = 2; i < len; i++) {
      if (chars[i] == '\n') {
        jscript = natus_new_string_utf16_length(ths, chars + i, len - i);
        break;
      }
    }
  }
  free(chars);

  // Add the file's directory to the require stack
  bool pushed = false;
  natusValue *glbl = natus_get_global(ths);
  natusValue *reqr = natus_get_utf8(glbl, "require");
  natusValue *stck = natus_get_private_name_value(reqr, "natus.reqStack");
  if (natus_is_array(stck))
    natus_decref(natus_set_index(stck, natus_as_long(natus_get_utf8(stck, "length")), filename));

  callandmkval(natusValue *rslt,
      natusValueTypeUnknown,
      ths, evaluate,
      ths->ctx->ctx,
      ths->val,
      jscript
      ? jscript->val
      : (javascript ? javascript->val : NULL),
      filename
      ? filename->val
      : NULL,
      lineno);
  natus_decref(jscript);

  // Remove the directory from the stack
  if (pushed)
    natus_decref(natus_call_utf8_array(stck, "pop", NULL));
  natus_decref(stck);
  natus_decref(reqr);
  return rslt;
}

natusValue *
natus_evaluate_utf8(natusValue *ths, const char *javascript, const char *filename, unsigned int lineno)
{
  if (!ths || !javascript)
    return NULL;
  if (!(natus_get_type(ths) & (natusValueTypeArray | natusValueTypeFunction | natusValueTypeObject)))
    return NULL;

  natusValue *jscript = natus_new_string_utf8(ths, javascript);
  if (!jscript)
    return NULL;

  natusValue *fname = NULL;
  if (filename) {
    fname = natus_new_string_utf8(ths, filename);
    if (!fname) {
      natus_decref(jscript);
      return NULL;
    }
  }

  natusValue *ret = natus_evaluate(ths, jscript, fname, lineno);
  natus_decref(jscript);
  natus_decref(fname);
  return ret;
}

natusValue *
natus_call(natusValue *func, natusValue *ths, ...)
{
  va_list ap;
  va_start(ap, ths);
  natusValue *tmp = natus_call_varg(func, ths, ap);
  va_end(ap);
  return tmp;
}

natusValue *
natus_call_varg(natusValue *func, natusValue *ths, va_list ap)
{
  natusValue *array = natus_new_array_varg(func, ap);
  if (natus_is_exception(array))
    return array;

  natusValue *ret = natus_call_array(func, ths, array);
  natus_decref(array);
  return ret;
}

natusValue *
natus_call_array(natusValue *func, natusValue *ths, natusValue *args)
{
  natusValue *newargs = NULL;
  if (!args)
    args = newargs = natus_new_array(func, NULL);
  if (ths)
    natus_incref(ths);
  else
    ths = natus_new_undefined(func);

  // If the function is native, skip argument conversion and js overhead;
  //    call directly for increased speed
  natusNativeFunction fnc = natus_get_private_name(func, NATUS_PRIV_FUNCTION);
  natusClass *cls = natus_get_private_name(func, NATUS_PRIV_CLASS);
  natusValue *res = NULL;
  if (fnc || (cls && cls->call)) {
    if (fnc)
      res = fnc(func, ths, args);
    else
      res = cls->call(cls, func, ths, args);
  } else if (natus_is_function(func)) {
    callandmkval(res, natusValueTypeUnknown, func, call, func->ctx->ctx, func->val, ths->val, args->val);
  }

  natus_decref(ths);
  natus_decref(newargs);
  return res;
}

natusValue *
natus_call_utf8(natusValue *ths, const char *name, ...)
{
  va_list ap;
  va_start(ap, name);
  natusValue *tmp = natus_call_utf8_varg(ths, name, ap);
  va_end(ap);
  return tmp;
}

natusValue *
natus_call_utf8_varg(natusValue *ths, const char *name, va_list ap)
{
  natusValue *array = natus_new_array_varg(ths, ap);
  if (natus_is_exception(array))
    return array;

  natusValue *ret = natus_call_utf8_array(ths, name, array);
  natus_decref(array);
  return ret;
}

natusValue *
natus_call_name(natusValue *ths, const natusValue *name, natusValue *args)
{
  natusValue *func = natus_get(ths, name);
  if (!func)
    return NULL;

  natusValue *ret = natus_call_array(func, ths, args);
  natus_decref(func);
  return ret;
}

natusValue *
natus_call_new(natusValue *func, ...)
{
  va_list ap;
  va_start(ap, func);
  natusValue *tmp = natus_call_new_varg(func, ap);
  va_end(ap);
  return tmp;
}

natusValue *
natus_call_new_varg(natusValue *func, va_list ap)
{
  natusValue *array = natus_new_array_varg(func, ap);
  if (natus_is_exception(array))
    return array;

  natusValue *ret = natus_call_new_array(func, array);
  natus_decref(array);
  return ret;
}

natusValue *
natus_call_utf8_array(natusValue *ths, const char *name, natusValue *args)
{
  natusValue *func = natus_get_utf8(ths, name);
  if (!func)
    return NULL;

  natusValue *ret = natus_call_array(func, ths, args);
  natus_decref(func);
  return ret;
}

natusValue *
natus_call_new_utf8(natusValue *obj, const char *name, ...)
{
  va_list ap;
  va_start(ap, name);
  natusValue *tmp = natus_call_new_utf8_varg(obj, name, ap);
  va_end(ap);
  return tmp;
}

natusValue *
natus_call_new_utf8_varg(natusValue *obj, const char *name, va_list ap)
{
  natusValue *array = natus_new_array_varg(obj, ap);
  if (natus_is_exception(array))
    return array;

  natusValue *ret = natus_call_new_utf8_array(obj, name, array);
  natus_decref(array);
  return ret;
}

natusValue *
natus_call_new_array(natusValue *func, natusValue *args)
{
  return natus_call_array(func, NULL, args);
}

natusValue *
natus_call_new_utf8_array(natusValue *obj, const char *name, natusValue *args)
{
  natusValue *fnc = natus_get_utf8(obj, name);
  natusValue *ret = natus_call_array(fnc, NULL, args);
  natus_decref(fnc);
  return ret;
}

bool
natus_equals(const natusValue *val1, const natusValue *val2)
{
  if (!val1 && !val2)
    return true;
  if (!val1 && natus_is_undefined(val2))
    return true;
  if (!val2 && natus_is_undefined(val1))
    return true;
  if (!val1 || !val2)
    return false;
  return val1->ctx->eng->spec->equal(val1->ctx->ctx, val1->val, val1->val, false);
}

bool
natus_equals_strict(const natusValue *val1, const natusValue *val2)
{
  if (!val1 && !val2)
    return true;
  if (!val1 && natus_is_undefined(val2))
    return true;
  if (!val2 && natus_is_undefined(val1))
    return true;
  if (!val1 || !val2)
    return false;
  return val1->ctx->eng->spec->equal(val1->ctx->ctx, val1->val, val1->val, true);
}

/**** ENGINE FUNCTIONS ****/

static natusEngVal
return_ownership(natusValue *val, natusEngValFlags *flags)
{
  natusEngVal ret = NULL;
  if (!flags)
    return NULL;
  *flags = natusEngValFlagNone;
  if (natus_is_exception(val))
    *flags |= natusEngValFlagException;
  if (!val)
    return NULL;

  /* If this value will free here, return ownership */
  ret = val->val;
  if (val->ref <= 1) {
    *flags |= natusEngValFlagMustFree;
    val->flg = val->flg & ~natusEngValFlagMustFree; /* Don't free this! */
  }
  natus_decref(val);

  return ret;
}

natusEngVal
natus_handle_property(const natusPropertyAction act, natusEngVal obj, const natusPrivate *priv, natusEngVal idx, natusEngVal val, natusEngValFlags *flags)
{
  natusValue *glbl = natus_private_get(priv, NATUS_PRIV_GLOBAL);
  assert(glbl);

  /* Convert the arguments */
  natusValue *vobj = mkval(glbl, obj, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vidx = mkval(glbl, act & natusPropertyActionEnumerate ? NULL : idx, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vval = mkval(glbl, act & natusPropertyActionSet ? val : NULL, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *rslt = NULL;

  /* Do the call */
  natusClass *clss = natus_private_get(priv, NATUS_PRIV_CLASS);
  if (clss && vobj && (vidx || act & natusPropertyActionEnumerate) && (vval || act & ~natusPropertyActionSet)) {
    switch (act) {
    case natusPropertyActionDelete:
      rslt = clss->del(clss, vobj, vidx);
      break;
    case natusPropertyActionGet:
      rslt = clss->get(clss, vobj, vidx);
      break;
    case natusPropertyActionSet:
      rslt = clss->set(clss, vobj, vidx, vval);
      break;
    case natusPropertyActionEnumerate:
      rslt = clss->enumerate(clss, vobj);
      break;
    default:
      assert(false);
    }
  }
  natus_decref(vobj);
  natus_decref(vidx);
  natus_decref(vval);

  return return_ownership(rslt, flags);
}

natusEngVal
natus_handle_call(natusEngVal obj, const natusPrivate *priv, natusEngVal ths, natusEngVal arg, natusEngValFlags *flags)
{
  natusValue *glbl = natus_private_get(priv, NATUS_PRIV_GLOBAL);
  assert(glbl);

  /* Convert the arguments */
  natusValue *vobj = mkval(glbl, obj, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vths = ths ? mkval(glbl, ths, natusEngValFlagMustFree, natusValueTypeUnknown) : natus_new_undefined(glbl);
  natusValue *varg = mkval(glbl, arg, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *rslt = NULL;
  if (vobj && vths && varg) {
    natusClass *clss = natus_private_get(priv, NATUS_PRIV_CLASS);
    natusNativeFunction func = natus_private_get(priv, NATUS_PRIV_FUNCTION);
    if (clss)
      rslt = clss->call(clss, vobj, vths, varg);
    else if (func)
      rslt = func(vobj, vths, varg);
  }

  /* Free the arguments */
  natus_decref(vobj);
  natus_decref(vths);
  natus_decref(varg);

  return return_ownership(rslt, flags);
}
