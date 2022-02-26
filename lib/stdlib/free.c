#include <stdlib.h>

/**
 * Deallocate space pointed to by ptr.
 *
 * @param ptr Pointer to the memory block to deallocate. Must be a pointer
 *            earlier returned by calloc(), malloc(), or realloc(), or NULL
 *            (in the later case no action will occur).
 */
void
free(void *ptr)
{
  struct __BlkHeader *hdr, *p;

  if (ptr == NULL)
    return;

  hdr = (struct __BlkHeader *) ptr - 1;

  // Scan the free list looking for a place to insert the block.
  for (p = __alloc_free; !((p < hdr) && (p->next > hdr)); p = p->next)
    if ((p >= p->next) && ((p < hdr) || (p->next > hdr)))
      break;

  if ((hdr + hdr->size) == p->next) {
    // Merge with the upper block.
    hdr->size += p->next->size;
    hdr->next = p->next->next;
  } else {
    // Insert before
    hdr->next = p->next;
  }

  if ((p + p->size) == hdr) {
    // Merge with the lower block
    p->size += hdr->size;
    p->next = hdr->next;
  } else {
    // Insert after
    p->next = hdr;
  }

  __alloc_free = p;
}
