#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "xalloc.h"

void *
realloc(void *ptr, size_t size)
{
  struct __BlkHeader *hdr, *p;
  size_t n;
  void *newptr;

  if (ptr == NULL)
    return malloc(size);

  hdr = (struct __BlkHeader *) ptr - 1;
  n = (size + sizeof(struct __BlkHeader) - 1) / sizeof(struct __BlkHeader) + 1;

  if (hdr->size == n)
    return ptr;
  
  if (n < hdr->size) {
    // Shrink the old block.
    p = hdr + n;
    p->size = hdr->size - n;

    hdr->size = n;
    hdr->next = p;

    free(p + 1);

    return ptr;
  }

  if ((newptr = malloc(size)) == NULL)
    return NULL;

  memmove(newptr, ptr, (hdr->size - 1) * sizeof(struct __BlkHeader));

  free(ptr);

  return newptr;
}
