#include <natus-internal.h>

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

natusValue *
natus_to_exception(natusValue *val)
{
  if (!val)
    return NULL;
  val->flg |= natusEngValFlagException;
  return val;
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
