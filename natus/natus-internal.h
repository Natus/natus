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

#ifndef NATUS_INTERNAL_H_
#define NATUS_INTERNAL_H_
#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-engine.h>

#define NATUS_PRIV_CLASS     "natus::Class"
#define NATUS_PRIV_FUNCTION  "natus::Function"
#define NATUS_PRIV_GLOBAL    "natus::Global"

#define callandmkval(n, t, c, f, ...) \
  natusEngValFlags _flags = natusEngValFlagUnlock | natusEngValFlagFree; \
  natusEngVal _val = c->ctx->spec->f(__VA_ARGS__, &_flags); \
  n = mkval(c, _val, _flags, t);

#define callandreturn(t, c, f, ...) \
  callandmkval(natusValue *_value, t, c, f, __VA_ARGS__); \
  return _value

typedef struct natusContext natusContext;
struct natusContext {
  natusEngCtx      ctx;
  natusEngineSpec *spec;
};

struct natusValue {
  natusContext    *ctx;
  natusEngVal      val;
  natusEngValFlags flag;
  natusValueType   type;
};

natusValue *
mkval(const natusValue *ctx, natusEngVal val, natusEngValFlags flags, natusValueType type);

void *
private_get(const natusPrivate *self, const char *name);

bool
private_set(natusPrivate *self, const char *name, void *priv, natusFreeFunction free);

#endif /* NATUS_INTERNAL_H_ */

