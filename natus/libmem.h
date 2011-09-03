/*
 * Copyright (c) 2011 Nathaniel McCallum <nathaniel@natemccallum.com>
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
 */

#ifndef LIBMEM_H_
#define LIBMEM_H_
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef void (*memFree)(void *);
typedef bool (*memForEach)(void *parent, void *child, void *data);

#define mem_new(prnt, type) \
  ((type*) mem_new_array_size(prnt, sizeof(type), 1))

#define mem_new_zero(prnt, type) \
  ((type*) mem_new_array_size_zero(prnt, sizeof(type), 1))

#define mem_new_size(prnt, size) \
  mem_new_array_size(prnt, size, 1)

#define mem_new_size_zero(prnt, size) \
  mem_new_array_size_zero(prnt, size, 1)

#define mem_new_array(prnt, type, cnt) \
  ((type*) mem_new_array_size(prnt, sizeof(type), cnt))

#define mem_new_array_zero(prnt, type, cnt) \
  ((type*) mem_new_array_size_zero(prnt, sizeof(type), cnt))

void *
mem_new_array_size(void *parent, size_t size, size_t count);

void *
mem_new_array_size_zero(void *parent, size_t size, size_t count);

#define mem_resize_array(mem, count) \
  mem_resize_array_size(mem, sizeof(__typeof__(**mem)), count)

bool
mem_resize_array_size(void **mem, size_t size, size_t count);

#define mem_resize_array_zero(mem, count) \
  mem_resize_array_size_zero(mem, sizeof(__typeof__(**mem)), count)

bool
mem_resize_array_size_zero(void **mem, size_t size, size_t count);

#define mem_incref(p, c) ((__typeof__(c)) _mem_incref(p, c))
void *
_mem_incref(void *parent, void *child);

void
mem_decref(void *parent, void *child);

void
mem_decref_children(void *parent);

void
mem_decref_children_name(void *parent, const char *name);

bool
mem_free(void *mem);

void
mem_group(void *cousin, void *mem);

size_t
mem_parents_count(void *mem, const char *name);

#define mem_parents_foreach(mem, name, cb, data) \
  _mem_parents_foreach(mem, name, (memForEach) cb, data)
void
_mem_parents_foreach(void *mem, const char *name, memForEach cb, void *data);

size_t
mem_children_count(void *mem, const char *name);

#define mem_children_foreach(mem, name, cb, data) \
  _mem_children_foreach(mem, name, (memForEach) cb, data)
void
_mem_children_foreach(void *mem, const char *name, memForEach cb, void *data);

#define mem_steal(p, c) ((__typeof__(c)) _mem_steal(p, c))
void *
_mem_steal(void *parent, void *child);

size_t
mem_size(void *mem);

#define mem_size_item(mem) \
  sizeof(__typeof__(*mem))

#define mem_size_items(mem) \
  (mem_size(mem) / mem_size_item(mem))

bool
mem_name_set(void *mem, const char *name);

const char *
mem_name_get(void *mem);

#define mem_destructor_set(mem, dstr) \
  _mem_destructor_set(mem, (memFree) dstr)
void
_mem_destructor_set(void *mem, memFree destructor);

char *
mem_strdup(void *parent, const char *str);

char *
mem_strndup(void *parent, const char *str, size_t len);

char *
mem_asprintf(void *parent, const char *fmt, ...);

char *
mem_vasprintf(void *parent, const char *fmt, va_list ap);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* LIBMEM_H_ */
