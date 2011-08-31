#include <natus-internal.h>

#include <assert.h>

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
  natusValue *glbl = private_get(priv, NATUS_PRIV_GLOBAL);
  assert(glbl);

  /* Convert the arguments */
  natusValue *vobj = mkval(glbl, obj, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vidx = mkval(glbl, act & natusPropertyActionEnumerate ? NULL : idx, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vval = mkval(glbl, act & natusPropertyActionSet ? val : NULL, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *rslt = NULL;

  /* Do the call */
  natusClass *clss = private_get(priv, NATUS_PRIV_CLASS);
  if (clss && vobj &&
      (vidx || (act & natusPropertyActionEnumerate)) &&
      (vval || (act & ~natusPropertyActionSet))) {
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
  natusValue *glbl = private_get(priv, NATUS_PRIV_GLOBAL);
  assert(glbl);

  /* Convert the arguments */
  natusValue *vobj = mkval(glbl, obj, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *vths = ths ? mkval(glbl, ths, natusEngValFlagMustFree, natusValueTypeUnknown) : natus_new_undefined(glbl);
  natusValue *varg = mkval(glbl, arg, natusEngValFlagMustFree, natusValueTypeUnknown);
  natusValue *rslt = NULL;
  if (vobj && vths && varg) {
    natusClass *clss = private_get(priv, NATUS_PRIV_CLASS);
    natusNativeFunction func = private_get(priv, NATUS_PRIV_FUNCTION);
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

void
natus_private_free(natusPrivate *priv)
{
  private_free(priv);
}

bool
natus_private_push(natusPrivate *priv, void *data, natusFreeFunction free)
{
  return private_set(priv, NULL, data, free);
}
