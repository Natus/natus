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
#include "enginemod.h"

typedef struct {
	char            *name;
	void            *priv;
	ntFreeFunction   free;
} ntPrivateItem;

struct _ntPrivate {
	ntPrivateItem  **priv;
};

ntPrivate *nt_private_init() {
	ntPrivate *priv = malloc(sizeof(ntPrivate));
	if (priv)
		priv->priv = NULL;
	return priv;
}

void nt_private_free(ntPrivate *self) {
	if (!self) return;

	// Free the privates
	int i;
	for (i=0 ; self->priv && self->priv[i] ; i++) {
		if (self->priv[i]->free && self->priv[i]->priv)
			self->priv[i]->free(self->priv[i]->priv);
		free(self->priv[i]->name);
		free(self->priv[i]);
	}
	free(self->priv);

	// Free the CFP
	free(self);
}

void *nt_private_get(ntPrivate *self, const char *name) {
	int i;
	if (!self || !name) return NULL;

	// Find an existing match and update it
	for (i=0 ; self->priv && self->priv[i] ; i++)
		if (!strcmp(self->priv[i]->name, name))
			return self->priv[i]->priv;
	return NULL;
}

bool nt_private_set(ntPrivate *self, const char *name, void *priv, ntFreeFunction free) {
	int i=0;
	if (!self || !name) return NULL;

	// Allocate our first private if needed
	if (!self->priv) {
		self->priv = malloc(sizeof(ntPrivateItem *));
		if (!self->priv) {
			if (free && priv) free(priv);
			return false;
		}
		self->priv[0] = NULL;
	}

	// Find an existing match and update it
	for (i=0 ; self->priv[i] ; i++) {
		if (!strcmp(self->priv[i]->name, name)) {
			if (self->priv[i]->priv && self->priv[i]->free)
				self->priv[i]->free(self->priv[i]->priv);

			// It priv is NULL, remove the item from the array
			if (!priv) {
				free(self->priv[i]->name);
				free(self->priv[i]);
				for ( ; self->priv[i] ; i++)
					self->priv[i] = self->priv[i+1];
				return true;
			}

			// Update the item
			self->priv[i]->priv = priv;
			self->priv[i]->free = free;
			return true;
		}
	}

	// No existing match was found, so expand the array
	ntPrivateItem **tmp = (ntPrivateItem **) realloc(self->priv, sizeof(ntPrivateItem *) * (i+2));
	if (!tmp) {
		if (free && priv) free(priv);
		return false;
	}
	self->priv = tmp;

	// Add the new item on the end
	self->priv[i] = (ntPrivateItem *) malloc(sizeof(ntPrivateItem));
	if (!self->priv[i]) return false;
	self->priv[i]->name = strdup(name);
	self->priv[i]->priv = priv;
	self->priv[i]->free = free;
	if (!self->priv[i]->name) {
		free(self->priv[i]);
		self->priv[i] = NULL;
		if (free && priv) free(priv);
		return false;
	}
	self->priv[++i] = NULL; // Terminate the array

	return true;
}
