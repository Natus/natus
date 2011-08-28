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

#ifndef PRIVATE_H_
#define PRIVATE_H_
#include "types.h"
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct natusPrivate natusPrivate;

typedef void
(*natusPrivateForeach)(const char *name, void *priv, void *misc);

natusPrivate *
natus_private_init(void *misc, natusFreeFunction free);

void
natus_private_free(natusPrivate *priv);

void *
natus_private_get(const natusPrivate *self, const char *name);

bool
natus_private_set(natusPrivate *self, const char *name, void *priv, natusFreeFunction free);

bool
natus_private_push(natusPrivate *self, void *priv, natusFreeFunction free);

void
natus_private_foreach(const natusPrivate *self, bool rev, natusPrivateForeach foreach, void *misc);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* PRIVATE_H_ */
