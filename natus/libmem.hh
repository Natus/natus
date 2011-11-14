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

#ifndef LIBMEM_HH_
#define LIBMEM_HH_
#include <new>
#include <cstddef>

void* operator new(std::size_t) throw (std::bad_alloc);
void* operator new[](std::size_t) throw (std::bad_alloc);
void* operator new(std::size_t, const std::nothrow_t&) throw();
void* operator new[](std::size_t, const std::nothrow_t&) throw();
void* operator new(std::size_t, void* parent, bool zero=false) throw (std::bad_alloc);
void* operator new[](std::size_t, void* parent, bool zero=false) throw (std::bad_alloc);
void* operator new(std::size_t, const std::nothrow_t&, void* parent, bool zero=false) throw();
void* operator new[](std::size_t, const std::nothrow_t&, void* parent, bool zero=false) throw();
void operator delete(void*) throw();
void operator delete[](void*) throw();
void operator delete(void*, const std::nothrow_t&) throw();
void operator delete[](void*, const std::nothrow_t&) throw();

namespace mem
{
typedef void (*Free)(void *);
typedef bool (*ForEach)(void *parent, void *child, void *data);

#define incref(p, c) ((__typeof__(c)) _incref(p, c))
void *
_incref(void *parent, void *child);

void
decref(void *parent, void *child);

void
decref_children(void *parent, const char *name=NULL);

void
free(void *mem);

void
group(void *cousin, void *mem);

size_t
parents_count(void *mem, const char *name=NULL);

#define parents_foreach(mem, name, cb, data) \
  _parents_foreach(mem, name, (memForEach) cb, data)
void
_parents_foreach(void *mem, const char *name, ForEach cb, void *data);

size_t
children_count(void *mem, const char *name=NULL);

#define children_foreach(mem, name, cb, data) \
  _children_foreach(mem, name, (memForEach) cb, data)
void
_children_foreach(void *mem, const char *name, ForEach cb, void *data);

#define steal(p, c) ((__typeof__(c)) _mem_steal(p, c))
void *
_steal(void *parent, void *child);

size_t
size(void *mem);

#define size_item(mem) \
  sizeof(__typeof__(*mem))

#define size_items(mem) \
  (size(mem) / size_item(mem))

bool
name_set(void *mem, const char *name);

const char *
name_get(void *mem);

#define destructor_set(mem, dstr) \
  _destructor_set(mem, (memFree) dstr)
void
_destructor_set(void *mem, Free destructor);
}

#endif /* LIBMEM_HH_ */
