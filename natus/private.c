/*
 * Copyright (c) 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-internal.h>

#include <libmem.h>
#include <assert.h>

typedef struct {
  void   *priv;
  memFree free;
} item;

static bool
foreach_get(void *parent, item *child, void **data)
{
  *data = child->priv;
  return false;
}

static bool
foreach_free(void *parent, void *child, void *data)
{
  mem_decref(parent, child);
  return true;
}

static void
item_dtor(item *itm)
{
  if (itm && itm->priv && itm->free)
    itm->free(itm->priv);
}

void *
private_get(const natusPrivate *self, const char *name)
{
  void *tmp = NULL;
  if (self && name)
    mem_children_foreach((natusPrivate*) self, name, foreach_get, &tmp);
  return tmp;
}

bool
private_set(natusPrivate *self, const char *name, void *priv, natusFreeFunction free)
{
  if (!self)
    return false;

  if (name)
    mem_children_foreach(self, name, foreach_free, NULL);

  item *itm = mem_new(self, item);
  if (itm) {
    if (name && !mem_name_set(itm, name)) {
      mem_decref(self, itm);
      return false;
    }
    mem_destructor_set(itm, item_dtor);
    itm->priv = priv;
    itm->free = free;
  }

  return itm != NULL;
}

bool
natus_set_private_name(natusValue *obj, const char *key, void *priv, natusFreeFunction free)
{
  if (!natus_is_type(obj, natusValueTypeSupportsPrivate))
    return false;
  if (!key)
    return false;

  natusPrivate *prv = obj->ctx->spec->get_private(obj->ctx->ctx, obj->val);
  return private_set(prv, key, priv, free);
}

static void
free_private_value(natusValue *pv)
{
  if (!pv)
    return;

  /* If the refcount is 0 it means that we are in the process of teardown,
   * and the value has already been unlocked because we are dismantling ctx.
   * Thus, we only do unlock if we aren't in this teardown phase. */
  if (mem_parents_count(pv->ctx, NULL) > 0)
    pv->ctx->spec->val_unlock(pv->ctx->ctx, pv->val);
  mem_free(pv);
}

bool
natus_set_private_name_value(natusValue *obj, const char *key, natusValue* priv)
{
  natusValue *val = NULL;

  if (priv) {
    /* NOTE: So we have this circular problem where if we hide just a normal
     *       natusValue* it keeps a reference to the ctx, but the hidden value
     *       won't be free'd until the ctx is destroyed, but that won't happen
     *       because we have a reference.
     *
     *       So the way around this is that we don't store the normal natusValue,
     *       but instead store a special one without a refcount on the ctx.
     *       This means that ctx will be properly freed. */
    val = mkval(priv, priv->ctx->spec->val_duplicate(priv->ctx->ctx, priv->val),
                priv->flag, priv->type);
    if (!val)
      return false;

    /* Don't keep a copy of this reference around,
     * guaranteed not to free in this case since we
     * just added an additional link in mkval() */
    mem_decref(val, val->ctx);
  }

  if (!natus_set_private_name(obj, key, val, (natusFreeFunction) free_private_value)) {
    free_private_value(val);
    return false;
  }
  return true;
}

void*
natus_get_private_name(const natusValue *obj, const char *key)
{
  if (!natus_is_type(obj, natusValueTypeSupportsPrivate))
    return NULL;
  if (!key)
    return NULL;
  natusPrivate *prv = obj->ctx->spec->get_private(obj->ctx->ctx, obj->val);
  return private_get(prv, key);
}

natusValue *
natus_get_private_name_value(const natusValue *obj, const char *key)
{
  /* See note in natus_set_private_name_value() */
  natusValue *val = natus_get_private_name(obj, key);
  if (!val)
    return NULL;
  return mkval(val,
      val->ctx->spec->val_duplicate(val->ctx->ctx, val->val),
      val->flag,
      val->type);
}
