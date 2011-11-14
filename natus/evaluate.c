#include <natus-internal.h>

#include <libmem.h>

#include <string.h>
#include <assert.h>

struct evalHook {
  evalHook *next;
  natusEvaluateHook hook;
  natusFreeFunction free;
  void *misc;
};

natusValue *
natus_evaluate(natusValue *ths, natusValue *javascript, natusValue *filename, unsigned int lineno)
{
  evalHook *tmp;

  if (!ths || !javascript)
    return NULL;

  natus_incref(javascript);
  natus_incref(filename);
  for (tmp = ths->ctx->evalhooks ; tmp ; tmp = tmp->next) {
    tmp->hook(ths, &javascript, &filename, &lineno, tmp->misc);
    assert(javascript);
  }

  callandmkval(natusValue *rslt, natusValueTypeUnknown, ths, evaluate,
               ths->ctx->ctx, ths->val, javascript->val,
               filename ? filename->val : NULL, lineno);

  for (tmp = ths->ctx->evalhooks ; tmp ; tmp = tmp->next)
    tmp->hook(ths, &rslt, &filename, NULL, tmp->misc);

  natus_decref(javascript);
  natus_decref(filename);
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

static void
hook_dtor(evalHook *hook)
{
  if (hook && hook->misc && hook->free)
    hook->free(hook->misc);
}

bool
natus_evaluate_hook_add(natusValue *ctx, const char *name,
                        natusEvaluateHook hook, void *misc,
                        natusFreeFunction free)
{
  evalHook *tmp;

  tmp = mem_new_zero(ctx->ctx, evalHook);
  if (!tmp || !mem_name_set(tmp, name)) {
    mem_free(tmp);
    if (misc && free)
      (*free)(misc);
    return false;
  }
  mem_destructor_set(tmp, hook_dtor);

  natus_evaluate_hook_del(ctx, name);

  tmp->misc = misc;
  tmp->free = free;
  tmp->hook = hook;
  tmp->next = ctx->ctx->evalhooks;
  ctx->ctx->evalhooks = tmp;

  return true;
}

bool
natus_evaluate_hook_del(natusValue *ctx, const char *name)
{
  evalHook *curr, *prev;

  for (prev = NULL, curr = ctx->ctx->evalhooks ; curr ; prev = curr, curr = curr->next) {
    if (!strcmp(mem_name_get(curr), name)) {
      if (!prev)
        ctx->ctx->evalhooks = curr->next;
      else
        prev->next = curr->next;
      mem_free(curr);
      return true;
    }
  }

  return false;
}
