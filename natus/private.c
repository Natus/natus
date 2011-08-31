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

#include <stdlib.h>
#include <string.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-internal.h>

typedef struct {
  char *name;
  void *priv;
  natusFreeFunction free;
} item;

struct natusPrivate {
  item  *priv;
  size_t size;
  size_t used;
  void  *misc;
  natusFreeFunction free;
};

static inline bool
_private_push(natusPrivate *self, const char *name, void *priv, natusFreeFunction free)
{
  if (!self || !priv) {
    if (free && priv)
      free(priv);
    return false;
  }

  // Expand the array if necessary
  if (self->used >= self->size) {
    self->size *= 2;
    item *tmp = realloc(self->priv, sizeof(item) * self->size);
    if (!tmp) {
      self->size /= 2;
      if (free && priv)
        free(priv);
      return false;
    }
    self->priv = tmp;
  }

  // Add the new item on the end
  self->priv[self->used].name = name ? strdup(name) : NULL;
  self->priv[self->used].priv = priv;
  self->priv[self->used].free = free;
  if (name && !self->priv[self->used].name) {
    if (free && priv)
      free(priv);
    return false;
  }

  self->used++;
  return true;
}

natusPrivate *
private_init(void *misc, natusFreeFunction free)
{
  natusPrivate *self = malloc(sizeof(natusPrivate));
  if (!self) {
    if (free)
      free(misc);
    return NULL;
  }

  self->free = free;
  self->misc = misc;
  self->used = 0;
  self->size = 8;
  self->priv = calloc(self->size, sizeof(item));
  if (!self->priv) {
    if (free)
      free(misc);
    free(self);
    return NULL;
  }
  return self;
}

void
private_free(natusPrivate *self)
{
  if (!self)
    return;

  // Free the privates
  size_t i;
  for (i = 0; i < self->used; i++) {
    if (self->priv[i].free && self->priv[i].priv)
      self->priv[i].free(self->priv[i].priv);
    free(self->priv[i].name);
  }
  free(self->priv);

  // Call the callback
  if (self->free)
    self->free(self->misc);

  // Free the CFP
  free(self);
}

void *
private_get(const natusPrivate *self, const char *name)
{
  size_t i;
  if (!self || !name)
    return NULL;

  // Find an existing match and update it
  for (i = 0; i < self->used; i++)
    if (self->priv[i].name && !strcmp(self->priv[i].name, name))
      return self->priv[i].priv;
  return NULL;
}

bool
private_set(natusPrivate *self, const char *name, void *priv, natusFreeFunction freef)
{
  size_t i = 0;
  if (!self) {
    if (freef && priv)
      freef(priv);
    return NULL;
  }

  // Find an existing match and update it
  for (i = 0; name && i < self->size; i++) {
    if (!self->priv[i].name)
      continue;
    if (strcmp(self->priv[i].name, name))
      continue;

    // We have a match, so free the item
    if (self->priv[i].priv && self->priv[i].free)
      self->priv[i].free(self->priv[i].priv);

    // It priv is NULL, remove the item from the array
    if (!priv) {
      free(self->priv[i].name);
      for (i++; i < self->size; i++)
        self->priv[i - 1] = self->priv[i];
      self->used--;
      return true;
    }

    // Update the item
    self->priv[i].priv = priv;
    self->priv[i].free = freef;
    return true;
  }

  // No existing match was found. We'll add a new item,
  return _private_push(self, name, priv, freef);
}

void
private_foreach(const natusPrivate *self, bool rev, privateForeach foreach, void *misc)
{
  ssize_t i;
  for (i = rev ? self->used - 1 : 0; rev ? i >= 0 : i < self->used; i += rev ? -1 : 1)
    foreach(self->priv[i].name, self->priv[i].priv, misc);
}

bool
natus_set_private_name(natusValue *obj, const char *key, void *priv, natusFreeFunction free)
{
  if (!natus_is_type(obj, natusValueTypeSupportsPrivate))
    return false;
  if (!key)
    return false;

  natusPrivate *prv = obj->ctx->eng->spec->get_private(obj->ctx->ctx, obj->val);
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
  if (pv->ctx->ref > 0)
    pv->ctx->eng->spec->val_unlock(pv->ctx->ctx, pv->val);
  pv->ctx->eng->spec->val_free(pv->val);
  free(pv);
}

bool
natus_set_private_name_value(natusValue *obj, const char *key, natusValue* priv)
{
  natusEngVal v = NULL;
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
    v = priv->ctx->eng->spec->val_duplicate(priv->ctx->ctx, priv->val);
    if (!v)
      return false;

    val = mkval(priv, v, priv->flg, priv->typ);
    if (!val)
      return false;
    val->ctx->ref--; /* Don't keep a copy of this reference around */
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
  natusPrivate *prv = obj->ctx->eng->spec->get_private(obj->ctx->ctx, obj->val);
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
      val->ctx->eng->spec->val_duplicate(val->ctx->ctx, val->val),
      val->flg,
      val->typ);
}
