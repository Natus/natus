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

#include "types.h"
#include "private.h"

typedef struct {
	char *name;
	void *priv;
	ntFreeFunction free;
} ntPrivateItem;

struct _ntPrivate {
	ntPrivateItem *priv;
	size_t size;
	size_t used;
	void  *misc;
	ntFreeFunction free;
};

static inline bool _private_push (ntPrivate *self, const char *name, void *priv, ntFreeFunction free) {
	if (!self || !priv) {
		if (free && priv)
			free (priv);
		return false;
	}

	// Expand the array if necessary
	if (self->used >= self->size) {
		self->size *= 2;
		ntPrivateItem *tmp = realloc (self->priv, sizeof(ntPrivateItem) * self->size);
		if (!tmp) {
			self->size /= 2;
			if (free && priv)
				free (priv);
			return false;
		}
		self->priv = tmp;
	}

	// Add the new item on the end
	self->priv[self->used].name = name ? strdup (name) : NULL;
	self->priv[self->used].priv = priv;
	self->priv[self->used].free = free;
	if (name && !self->priv[self->used].name) {
		if (free && priv)
			free (priv);
		return false;
	}

	self->used++;
	return true;
}

ntPrivate *nt_private_init (void *misc, ntFreeFunction free) {
	ntPrivate *self = malloc (sizeof(ntPrivate));
	if (!self) {
          if (free)
                  free(misc);
          return NULL;
	}

        self->free = free;
        self->misc = misc;
        self->used = 0;
        self->size = 8;
        self->priv = calloc (self->size, sizeof(ntPrivateItem));
        if (!self->priv) {
                if (free)
                        free(misc);
                free (self);
                return NULL;
        }
	return self;
}

void nt_private_free (ntPrivate *self) {
	if (!self)
		return;

	// Free the privates
	size_t i;
	for (i = 0; i < self->used ; i++) {
		if (self->priv[i].free && self->priv[i].priv)
			self->priv[i].free (self->priv[i].priv);
		free (self->priv[i].name);
	}
	free (self->priv);

	// Call the callback
	if (self->free)
	        self->free(self->misc);

	// Free the CFP
	free (self);
}

void *nt_private_get (const ntPrivate *self, const char *name) {
	size_t i;
	if (!self || !name)
		return NULL;

	// Find an existing match and update it
	for (i = 0; i < self->used ; i++)
		if (self->priv[i].name && !strcmp (self->priv[i].name, name))
			return self->priv[i].priv;
	return NULL;
}

bool nt_private_set (ntPrivate *self, const char *name, void *priv, ntFreeFunction freef) {
	size_t i = 0;
	if (!self || !name) {
		if (freef && priv)
			freef (priv);
		return NULL;
	}

	// Find an existing match and update it
	for (i = 0; i < self->size ; i++) {
		if (!self->priv[i].name)
			continue;
		if (strcmp (self->priv[i].name, name))
			continue;

		// We have a match, so free the item
		if (self->priv[i].priv && self->priv[i].free)
			self->priv[i].free (self->priv[i].priv);

		// It priv is NULL, remove the item from the array
		if (!priv) {
			free (self->priv[i].name);
			for (i++; i < self->size ; i++)
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
	return _private_push (self, name, priv, freef);
}

bool nt_private_push (ntPrivate *self, void *priv, ntFreeFunction free) {
	// No existing match was found. We'll add a new item,
	return _private_push (self, NULL, priv, free);
}

void nt_private_foreach (const ntPrivate *self, bool rev, void(*foreach) (const char *name, void *priv, void *misc), void *misc) {
	ssize_t i;
	for (i = rev ? self->used - 1 : 0; rev ? i >= 0 : i < self->used ; i += rev ? -1 : 1)
		foreach (self->priv[i].name, self->priv[i].priv, misc);
}
