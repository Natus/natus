#include <natus-internal.h>

#include <string.h>

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

natusValue *
natus_push(natusValue *array, natusValue *val)
{
  if (!array || !val)
    return NULL;

  natusValue *length = natus_get_utf8(array, "length");
  if (!natus_is_exception(length) && natus_is_number(length)) {
    natusValue *rslt = natus_set_index(array, natus_as_long(length), val);
    if (natus_is_exception(rslt)) {
      natus_decref(rslt);
      return NULL;
    }
    natus_decref(rslt);
    return array;
  }

  natus_decref(length);
  return NULL;
}

natusValue *
natus_pop(natusValue *array)
{
  return natus_call_utf8(array, "pop", NULL);
}

