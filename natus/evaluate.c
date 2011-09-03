#include <natus-internal.h>
#include "evil.h"

static bool
push(natusValue *ctx, natusValue *filename)
{
  natusValue *glbl = natus_get_global(ctx);
  natusValue *stck = natus_get_private_name_value(glbl, NATUS_EVALUATION_STACK);
  if (!natus_is_array(stck)) {
    natus_decref(stck);

    stck = natus_new_array_vector(glbl, NULL);
    if (!natus_is_array(stck) ||
        !natus_set_private_name_value(glbl, NATUS_EVALUATION_STACK, stck)) {
      natus_decref(stck);
      return false;
    }
  }

  bool success = natus_push(stck, filename);
  natus_decref(stck);
  return success;
}

static void
pop(natusValue *ctx)
{
  natusValue *glbl = natus_get_global(ctx);
  natusValue *stck = natus_get_private_name_value(glbl, NATUS_EVALUATION_STACK);
  if (natus_is_array(stck))
    natus_pop(stck);
  natus_decref(stck);
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

  bool pushed = push(ths, (natusValue*) filename);
  callandmkval(natusValue *rslt, natusValueTypeUnknown, ths, evaluate,
               ths->ctx->ctx, ths->val,
               jscript ? jscript->val : (javascript ? javascript->val : NULL),
               filename ? filename->val : NULL, lineno);
  if (pushed)
    pop(ths);

  natus_decref(jscript);
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
