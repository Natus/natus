#include <natus-internal.h>

#include <assert.h>

#include <libmem.h>

#define hmkval(ctx, val) \
  mkval(ctx, val, \
        natusEngValFlagUnlock | natusEngValFlagFree, \
        natusValueTypeUnknown)

static natusEngVal
return_ownership(natusValue *val, natusEngValFlags *flags)
{
  natusEngVal ret = NULL;
  if (!flags)
    return NULL;
  *flags = natusEngValFlagException;
  if (!val)
    return NULL;
  *flags = val->flag;

  /* If this value will not free here, retain ownership */
  ret = val->val;
  if (mem_parents_count(val, NULL) > 1)
    *flags &= ~(natusEngValFlagUnlock | natusEngValFlagFree);
  else
    val->flag &= ~(natusEngValFlagUnlock | natusEngValFlagFree);
  natus_decref(val);

  return ret;
}

natusEngVal
natus_handle_property(const natusPropertyAction act, natusEngVal obj, const natusPrivate *priv, natusEngVal idx, natusEngVal val, natusEngValFlags *flags)
{
  natusValue *glbl = private_get(priv, NATUS_PRIV_GLOBAL);
  assert(glbl);

  /* Convert the arguments */
  natusValue *vobj = hmkval(glbl, obj);
  natusValue *vidx = hmkval(glbl, act & natusPropertyActionEnumerate ? NULL : idx);
  natusValue *vval = hmkval(glbl, act & natusPropertyActionSet ? val : NULL);
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
  natusValue *vobj = hmkval(glbl, obj);
  natusValue *vths = ths ? hmkval(glbl, ths) : natus_new_undefined(glbl);
  natusValue *varg = hmkval(glbl, arg);
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
  mem_free(priv);
}

bool
natus_private_push(natusPrivate *priv, void *data, natusFreeFunction free)
{
  return private_set(priv, NULL, data, free);
}
