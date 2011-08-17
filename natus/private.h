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
extern "C" {
#endif /* __cplusplus */

typedef struct _ntPrivate ntPrivate;
typedef void (*ntPrivateForeach) (const char *name, void *priv, void *misc);

ntPrivate *nt_private_init ();
void nt_private_free (ntPrivate *priv);
void *nt_private_get (const ntPrivate *self, const char *name);
bool nt_private_set (ntPrivate *self, const char *name, void *priv, ntFreeFunction free);
bool nt_private_push (ntPrivate *self, void *priv, ntFreeFunction free);
void nt_private_foreach (const ntPrivate *self, bool rev, ntPrivateForeach foreach, void *misc);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* PRIVATE_H_ */
