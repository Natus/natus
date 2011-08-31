#include <natus-internal.h>

#include <assert.h>

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
