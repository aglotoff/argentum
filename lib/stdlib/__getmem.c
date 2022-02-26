#include <stdlib.h>
#include <unistd.h>

#define ALLOC_MIN   (4096 / sizeof(struct __BlkHeader))

struct __BlkHeader *
__getmem(size_t nunits)
{
  struct __BlkHeader *hdr;
  void *p;

  if (nunits < ALLOC_MIN)
    nunits = ALLOC_MIN;

  if ((p = sbrk(nunits * sizeof(struct __BlkHeader))) == (void *) -1)
    return NULL;
  
  hdr       = (struct __BlkHeader *) p;
  hdr->size = nunits;

  // Insert the new block into the free list.
  free(hdr + 1);

  // Return the pointer to the new free list.
  return __alloc_free;
}
