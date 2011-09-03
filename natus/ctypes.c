#include <natus-internal.h>

bool
natus_to_bool(const natusValue *val)
{
  if (!val)
    return false;
  if (natus_is_exception(val))
    return false;

  return val->ctx->spec->to_bool(val->ctx->ctx, val->val);
}

double
natus_to_double(const natusValue *val)
{
  if (!val)
    return 0;
  return val->ctx->spec->to_double(val->ctx->ctx, val->val);
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
      char *tmp = val->ctx->spec->to_string_utf8(str->ctx->ctx, str->val, len);
      natus_decref(str);
      return tmp;
    }
    natus_decref(str);
  }

  return val->ctx->spec->to_string_utf8(val->ctx->ctx, val->val, len);
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
      natusChar *tmp = val->ctx->spec->to_string_utf16(str->ctx->ctx, str->val, len);
      natus_decref(str);
      return tmp;
    }
    natus_decref(str);
  }

  return val->ctx->spec->to_string_utf16(val->ctx->ctx, val->val, len);
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
