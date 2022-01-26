#ifndef __LIB_STDLIB_XALLOC_H__
#define __LIB_STDLIB_XALLOC_H__

#include <stddef.h>

struct __BlkHeader {
  struct __BlkHeader *next; // Next block (if on the free list)
  size_t              size; // Size of this block (in bytes)
};

extern struct __BlkHeader *__alloc_free; // Start of the free list

struct __BlkHeader *__getmem(size_t);

#endif  // !__LIB_STDLIB_XALLOC_H__
