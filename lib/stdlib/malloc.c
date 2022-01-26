#include <errno.h>
#include <stdlib.h>

#include "xalloc.h"

struct __BlkHeader *__alloc_free;
static struct __BlkHeader __alloc_base; 

/**
 * Allocate space for an object of the request size.
 *
 * @param size Size of the object, in bytes.
 * @return A pointer to the allocated space or NULL if not enough memory.
 */
void *
malloc(size_t size)
{
  struct __BlkHeader *p, *prevp;
  size_t n;

  if (size == 0)
    return NULL;

  // Round up to the nearest multiple of block header size + 1 for the header
  // itself.
  n = (size + sizeof(struct __BlkHeader) - 1) / sizeof(struct __BlkHeader) + 1;

  // If this is the first time, initialize the allocator.
  if (__alloc_free == NULL) {
    // Dummy block of size 0.
    __alloc_base.next = &__alloc_base;
    __alloc_base.size = 0;

    __alloc_free = &__alloc_base;
  }

  // Scan the free list using the first-fit approach. To keep the list
  // homogenous, begin search at the point where the last block was allocated.
  for (prevp = __alloc_free, p = prevp->next; ; prevp = p, p = p->next) {
    if (p->size >= n) {
      if (p->size == n) {
        // Exact fit
        prevp->next = p->next;
      } else {
        // Split a larger block
        p->size -= n;
        p += p->size;
        p->size = n;
      }

      // Rememeber this point so the next time search begins here.
      __alloc_free = prevp;

      return p + 1;
    }

    if (p ==__alloc_free) {
      // Wrapped around the free list, request more memory from the kernel.
      if ((p = __getmem(n)) == NULL) {
        errno = ENOMEM;
        return NULL;
      }
    }
  }
}
