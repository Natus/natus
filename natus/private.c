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
} natusPrivateItem;

struct natusPrivate {
  natusPrivateItem *priv;
  size_t size;
  size_t used;
  void *misc;
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
    natusPrivateItem *tmp = realloc(self->priv, sizeof(natusPrivateItem) * self->size);
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
natus_private_init(void *misc, natusFreeFunction free)
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
  self->priv = calloc(self->size, sizeof(natusPrivateItem));
  if (!self->priv) {
    if (free)
      free(misc);
    free(self);
    return NULL;
  }
  return self;
}

void
natus_private_free(natusPrivate *self)
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
natus_private_get(const natusPrivate *self, const char *name)
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
natus_private_set(natusPrivate *self, const char *name, void *priv, natusFreeFunction freef)
{
  size_t i = 0;
  if (!self || !name) {
    if (freef && priv)
      freef(priv);
    return NULL;
  }

  // Find an existing match and update it
  for (i = 0; i < self->size; i++) {
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

bool
natus_private_push(natusPrivate *self, void *priv, natusFreeFunction free)
{
  // No existing match was found. We'll add a new item,
  return _private_push(self, NULL, priv, free);
}

void
natus_private_foreach(const natusPrivate *self, bool rev, void
(*foreach)(const char *name, void *priv, void *misc), void *misc)
{
  ssize_t i;
  for (i = rev ? self->used - 1 : 0; rev ? i >= 0 : i < self->used; i += rev ? -1 : 1)
    foreach(self->priv[i].name, self->priv[i].priv, misc);
}
