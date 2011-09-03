#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include <natus-internal.h>

#include <libmem.h>

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

static bool
ctx_get_dll(void *parent, void **child, void ***data)
{
  *data = child;
  return false;
}

natusValue *
natus_new_function(const natusValue *ctx, natusNativeFunction func, const char *name)
{
  void **dll;
  if (!ctx || !func)
    return NULL;

  mem_children_foreach(ctx->ctx, "dll", ctx_get_dll, &dll);
  if (!dll)
    return NULL;

  natusPrivate *priv = mem_new_size(dll, 0);
  if (!priv)
    return NULL;

  if (!private_set(priv, NATUS_PRIV_GLOBAL, natus_get_global(ctx), NULL))
    goto error;

  if (!private_set(priv, NATUS_PRIV_FUNCTION, func, NULL))
    goto error;

  callandreturn(natusValueTypeFunction, ctx, new_function, ctx->ctx->ctx, name, priv);

error:
  mem_decref(dll, priv);
  return NULL;
}

natusValue *
natus_new_object(const natusValue *ctx, natusClass *cls)
{
  void **dll;
  if (!ctx)
    return NULL;

  mem_children_foreach(ctx->ctx, "dll", ctx_get_dll, &dll);
  if (!dll)
    return NULL;

  natusPrivate *priv = mem_new_size(dll, 0);
  if (!priv)
    return NULL;

  if (cls && !private_set(priv, NATUS_PRIV_GLOBAL, natus_get_global(ctx), NULL))
    goto error;

  if (cls && !private_set(priv, NATUS_PRIV_CLASS, cls, (natusFreeFunction) cls->free))
    goto error;

  callandreturn(natusValueTypeObject, ctx, new_object, ctx->ctx->ctx, cls, priv);

error:
  mem_decref(dll, priv);
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
