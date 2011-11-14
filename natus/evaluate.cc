#include <natus-internal.hh>

Value
Value::evaluate(Value javascript, Value filename, unsigned int lineno)
{
  return natus_evaluate(internal, javascript.internal, filename.internal, lineno);
}

Value
Value::evaluate(UTF8 javascript, UTF8 filename, unsigned int lineno)
{
  Value js = newString(javascript);
  Value fn = newString(filename);
  return natus_evaluate(internal, js.internal, fn.internal, lineno);
}

Value
Value::evaluate(UTF16 javascript, UTF16 filename, unsigned int lineno)
{
  Value js = newString(javascript);
  Value fn = newString(filename);
  return natus_evaluate(internal, js.internal, fn.internal, lineno);
}

struct hook_data {
  void *misc;
  FreeFunction free;
  EvaluateHook hook;
};

static void
cxx_hook_free(void *data) {
  hook_data *hd = (hook_data*) data;

  if (hd->free && hd->misc)
    hd->free(hd->misc);
  delete hd;
}

static void
cxx_hook(natusValue *ths, natusValue **javascript_or_return,
         natusValue **filename, unsigned int *lineno, void *misc) {
  Value vths(ths, false);
  Value vjsr(*javascript_or_return, true);
  Value vfnm(*filename, true);
  hook_data *hd = (hook_data*) misc;

  hd->hook(ths, &vjsr, &vfnm, lineno, hd->misc);

  *javascript_or_return = natus_incref(vjsr.borrowCValue());
  if (filename)
    *filename = natus_incref(vfnm.borrowCValue());
}

bool
addEvaluateHook(Value ctx, const char *name, EvaluateHook hook,
                void *misc, FreeFunction free)
{
  hook_data *hd;

  try {
    hd = new hook_data;
    hd->misc = misc;
    hd->free = free;
    hd->hook = hook;
  } catch (std::bad_alloc &e) {
    if (misc && free)
      free(misc);
    return false;
  }

  return natus_evaluate_hook_add(ctx.borrowCValue(), name, cxx_hook, hd, cxx_hook_free);
}

bool
delEvaluateHook(Value ctx, const char *name)
{
  return natus_evaluate_hook_del(ctx.borrowCValue(), name);
}
