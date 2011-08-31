#include <natus-internal.h>

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
