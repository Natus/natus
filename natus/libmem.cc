#include "libmem.hh"
#include "libmem.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
using namespace std;

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#define DEFAULT_LINK_SIZE 2

#define _GET_CHUNK(mem) \
  (mem >= sizeof(chunk) ? (mem - sizeof(chunk)) : 0)
#define GET_CHUNK(mem) \
  ((chunk*) _GET_CHUNK((uintptr_t) (mem)))
#define GET_ALLOC(chnk) \
  ((void*) (chnk ? chnk + 1 : NULL))
#define OR_MAX(n) \
  (n < UINT16_MAX ? n : UINT16_MAX)

struct chunk;

struct link {
  chunk **chunks;
  uint16_t size;
  uint16_t used;
};

struct chunk {
  link parents;
  link children;
  chunk *prev;
  chunk *next;
  size_t size;
  char *name;
  memFree destructor;
};

static bool
push(link* lnk, chunk *chnk)
{
  if (!lnk)
    return false;

  if (lnk->used == lnk->size) {
    size_t size = lnk->size > 0
                    ? OR_MAX(((size_t) lnk->size) * 2)
                    : DEFAULT_LINK_SIZE;
    /* Check to make sure we don't roll over our ref */
    if (size == lnk->size)
      return false;
    chunk **tmp = (chunk**) realloc(lnk->chunks, size * sizeof(chunk*));
    if (!tmp)
      return false;
    lnk->chunks = tmp;
    lnk->size = size;
  }

  lnk->chunks[lnk->used++] = chnk;
  return true;
}

static bool
pop(link *lnk, chunk *chnk)
{
  size_t i;
  if (!lnk || !lnk->chunks)
    return false;

  for (i = 0; i < lnk->used; i++) {
    if (lnk->chunks[i] == chnk) {
      lnk->chunks[i] = lnk->chunks[--lnk->used];
      return true;
    }
  }

  return false;
}


#define sib_loop(chnk, tmp, code) \
  for (chunk *step_, *tmp = chnk->prev; tmp; tmp = step_) { \
    step_ = tmp->prev; \
    code; \
  } \
  for (chunk *step_, *tmp = chnk; tmp; tmp = step_) { \
    step_ = tmp->next; \
    code; \
  }

static void
_mem_unlink(chunk *prnt, chunk *chld, bool bothsides)
{
  if (!chld)
    return;

  if (pop(&chld->parents, prnt) && prnt && bothsides)
    pop(&prnt->children, chld);

  size_t count = 0;
  sib_loop(chld, tmp, count += tmp->parents.used);
  if (count == 0) {
    /* First loop: call all destructors */
    sib_loop(chld, tmp,
      if (tmp->destructor)
        tmp->destructor(GET_ALLOC(tmp));
    );

    /* Second loop: remove the children, do the free */
    sib_loop(chld, tmp,
      for (size_t i=tmp->children.used; i > 0; i--)
        _mem_unlink(tmp, tmp->children.chunks[i-1], false);

      free(tmp->children.chunks);
      free(tmp->parents.chunks);
      free(tmp);
    );
  }
}

#define domalloc(sz, parent, zero, err) \
  chunk *chnk = (chunk*) malloc(sizeof(chunk) + (sz)); \
  if (!chnk) { err; } \
  memset(chnk, 0, sizeof(chunk)); \
  chnk->size = sz; \
  void *tmp = mem::_incref(parent, GET_ALLOC(chnk)); \
  if (!tmp) { \
    free(chnk); \
    err; \
  } \
  if (zero) \
    memset(tmp, 0, sz); \
  return tmp

void*
operator new(std::size_t size) throw (std::bad_alloc)
{
  domalloc(size, NULL, false, throw bad_alloc());
}

void*
operator new[](std::size_t size) throw (std::bad_alloc)
{
  domalloc(size, NULL, false, throw bad_alloc());
}

void*
operator new(std::size_t size, const std::nothrow_t&) throw ()
{
  domalloc(size, NULL, false, return NULL);
}

void*
operator new[](std::size_t size, const std::nothrow_t&) throw ()
{
  domalloc(size, NULL, false, return NULL);
}

void*
operator new(size_t size, void* parent, bool zero) throw (std::bad_alloc)
{
  domalloc(size, parent, zero, throw bad_alloc());
}

void*
operator new[](size_t size, void* parent, bool zero) throw (std::bad_alloc)
{
  domalloc(size, parent, zero, throw bad_alloc());
}

void*
operator new(size_t size, const std::nothrow_t&, void* parent, bool zero) throw ()
{
  domalloc(size, parent, zero, return NULL);
}

void*
operator new[](size_t size, const std::nothrow_t&, void* parent, bool zero) throw ()
{
  domalloc(size, parent, zero, return NULL);
}


void
operator delete(void* ptr) throw ()
{
  mem::decref(NULL, ptr);
}

void
operator delete[](void* ptr) throw ()
{
  mem::decref(NULL, ptr);
}

void
operator delete(void* ptr, const std::nothrow_t&) throw ()
{
  mem::decref(NULL, ptr);
}

void
operator delete[](void* ptr, const std::nothrow_t&) throw ()
{
  mem::decref(NULL, ptr);
}

void *
mem_new_array_size(void *parent, size_t size, size_t count)
{
  domalloc(size * count, parent, false, return NULL);
}

void *
mem_new_array_size_zero(void *parent, size_t size, size_t count)
{
  domalloc(size * count, parent, true, return NULL);
}

bool
mem_resize_array_size(void **mem, size_t size, size_t count)
{
  chunk *chnk = GET_CHUNK(mem ? *mem : NULL);
  if (!chnk)
    return false;
  chunk *tmp = (chunk*) realloc(chnk, sizeof(chunk) + (size * count));
  if (!tmp)
    return false;
  tmp->size = size * count;
  *mem = GET_ALLOC(tmp);
  return true;
}

bool
mem_resize_array_size_zero(void **mem, size_t size, size_t count)
{
  chunk *chnk = GET_CHUNK(mem ? *mem : NULL);
  if (!chnk)
    return false;
  size_t oldsize = chnk->size;

  if (!mem_resize_array_size(mem, size, count))
    return false;

  chnk = GET_CHUNK(mem ? *mem : NULL);
  if (!chnk)
    return false;
  if (size * count > oldsize)
    memset(((char *) *mem) + oldsize, 0, size * count - oldsize);
  return true;
}

void *
_mem_incref(void *parent, void *child)
{
  return mem::_incref(parent, child);
}

void
mem_decref(void *parent, void *child)
{
  return mem::decref(parent, child);
}

void
mem_decref_children(void *parent)
{
  mem::decref_children(parent);
}

void
mem_decref_children_name(void *parent, const char *name)
{
  mem::decref_children(parent, name);
}

void
mem_free(void *mem)
{
  return mem::free(mem);
}

void
mem_group(void *cousin, void *mem)
{
  mem::group(cousin, mem);
}

size_t
mem_parents_count(void *mem, const char *name)
{
  return mem::parents_count(mem, name);
}

void
_mem_parents_foreach(void *mem, const char *name, memForEach cb, void *data)
{
  mem::_parents_foreach(mem, name, cb, data);
}

size_t
mem_children_count(void *mem, const char *name)
{
  return mem::children_count(mem, name);
}

void
_mem_children_foreach(void *mem, const char *name, memForEach cb, void *data)
{
  mem::_children_foreach(mem, name, cb, data);
}

void *
_mem_steal(void *parent, void *child)
{
  return mem::_steal(parent, child);
}

size_t
mem_size(void *mem)
{
  return mem::size(mem);
}

bool
mem_name_set(void *mem, const char *name)
{
  return mem::name_set(mem, name);
}

const char *
mem_name_get(void *mem)
{
  return mem::name_get(mem);
}

void
_mem_destructor_set(void *mem, memFree destructor)
{
  mem::_destructor_set(mem, destructor);
}

char *
mem_strdup(void *parent, const char *str)
{
  if (!str)
    return NULL;
  return mem_strndup(parent, str, strlen(str));
}

char *
mem_strndup(void *parent, const char *str, size_t len)
{
  if (!str)
    return NULL;
  char *tmp = mem_new_array_zero(parent, char, len+1);
  strncpy(tmp, str, len);
  return tmp;
}

char *
mem_asprintf(void *parent, const char *fmt, ...)
{
  va_list ap;
  char *str;

  va_start(ap, fmt);
  str = mem_vasprintf(parent, fmt, ap);
  va_end(ap);
  return str;
}

char *
mem_vasprintf(void *parent, const char *fmt, va_list ap)
{
  va_list apc;
  ssize_t size = 0;
  char *str = NULL;

  va_copy(apc, ap);
  size = vsnprintf(NULL, 0, fmt, apc);
  va_end(apc);

  if (size <= 0 || !(str = mem_new_array(parent, char, size)))
    return NULL;

  vsnprintf(str, size + 1, fmt, ap);
  return str;
}

namespace mem
{
void *
_incref(void *parent, void *child)
{
  chunk *chld, *prnt;

  chld = GET_CHUNK(child);
  prnt = GET_CHUNK(parent);
  if (!chld)
    return NULL;

  if (!push(&(chld->parents), prnt))
    return NULL;

  if (prnt && !push(&(prnt->children), chld)) {
    pop(&(chld->parents), prnt);
    return NULL;
  }

  return child;
}

void
decref(void *parent, void *child)
{
  _mem_unlink(GET_CHUNK(parent), GET_CHUNK(child), true);
}

void
decref_children(void *parent, const char *name)
{
  chunk *chnk = GET_CHUNK(parent);
  if (!chnk)
    return;

  for (size_t i=chnk->children.used; i > 0; i--) {
    chunk *tmp = chnk->children.chunks[i-1];
    if (!name || (tmp->name && !strcmp(name, tmp->name)))
      _mem_unlink(chnk, tmp, true);
  }
}

void
free(void *mem)
{
  chunk *chnk = GET_CHUNK(mem);
  if (!chnk || chnk->parents.used != 1)
    return;
  _mem_unlink(chnk->parents.chunks[0], chnk, true);
  return;
}

void
group(void *cousin, void *mem)
{
  chunk *chnk = GET_CHUNK(mem);
  chunk *csnc = GET_CHUNK(cousin);
  if (!chnk || !csnc)
    return;

  chunk *head = chnk;
  chunk *tail = chnk;
  while (head->prev)
    head = head->prev;
  while (tail->next)
    tail = tail->next;

  if (csnc->next)
    csnc->next->prev = tail;
  tail->next = csnc->next;
  csnc->next = head;
  head->prev = csnc;
}

size_t
parents_count(void *mem, const char *name)
{
  chunk *chnk = GET_CHUNK(mem);
  if (!chnk)
    return 0;

  if (!name)
    return chnk->parents.used;

  size_t i, count;
  for (i=0, count=0; i < chnk->parents.used; i++)
    if (!strcmp(chnk->parents.chunks[i]->name, name))
      count++;

  return count;
}

void
_parents_foreach(void *mem, const char *name, memForEach cb, void *data)
{
  chunk *chnk = GET_CHUNK(mem);
  if (!chnk)
    return;

  for (size_t i=chnk->parents.used; i > 0; i--)
    if (!name || (chnk->parents.chunks[i-1]->name &&
                  !strcmp(chnk->parents.chunks[i-1]->name, name)))
      if (!cb(mem, GET_ALLOC(chnk->parents.chunks[i-1]), data))
        break;
}

size_t
children_count(void *mem, const char *name)
{
  chunk *chnk = GET_CHUNK(mem);
  if (!chnk)
    return 0;

  if (!name)
    return chnk->children.used;

  size_t i, count;
  for (i=0, count=0; i < chnk->children.used; i++)
    if (!strcmp(chnk->children.chunks[i]->name, name))
      count++;

  return count;
}

void
_children_foreach(void *mem, const char *name, memForEach cb, void *data)
{
  chunk *chnk = GET_CHUNK(mem);
  if (!chnk)
    return;

  for (size_t i=chnk->children.used; i > 0; i--)
    if (!name || (chnk->children.chunks[i-1]->name &&
                  !strcmp(chnk->children.chunks[i-1]->name, name)))
      if (!cb(mem, GET_ALLOC(chnk->children.chunks[i-1]), data))
        break;
}

void *
_steal(void *parent, void *child)
{
  chunk *chld = GET_CHUNK(child);
  if (!chld || chld->parents.used != 1)
    return NULL;

  if (chld->parents.chunks[0])
    pop(&chld->parents.chunks[0]->children, chld);
  chld->parents.chunks[0] = GET_CHUNK(parent);
  return child;
}

size_t
size(void *mem)
{
  chunk *chnk = GET_CHUNK(mem);
  if (chnk)
    return chnk->size;
  return 0;
}

bool
name_set(void *mem, const char *name)
{
  chunk *chnk = GET_CHUNK(mem);
  if (chnk)
    return (!name || (chnk->name = mem_strdup(mem, name)));
  return false;
}

const char *
name_get(void *mem)
{
  chunk *chnk = GET_CHUNK(mem);
  return chnk ? chnk->name : NULL;
}

void
_destructor_set(void *mem, memFree destructor)
{
  chunk *chnk = GET_CHUNK(mem);
  if (chnk)
    chnk->destructor = destructor;
}
}
