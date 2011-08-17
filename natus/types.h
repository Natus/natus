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

#ifndef TYPES_H_
#define TYPES_H_
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef _HAVE_NT_VALUE
#define _HAVE_NT_VALUE
typedef struct _ntValue ntValue;
#endif

#ifdef WIN32
typedef wchar_t ntChar;
#else
typedef uint16_t ntChar;
#endif

/* Type: ntFreeFunction
 * Function type for calls made back to free a memory value allocated outside natus.
 *
 * Parameters:
 *     mem - The memory to free.
 */
typedef void (*ntFreeFunction) (void *mem);
#endif /* TYPES_H_ */

