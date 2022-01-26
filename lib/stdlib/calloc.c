#include <stdlib.h>
#include <string.h>

/**
 * Allocates space for an array of 'nmemb' objects, each of whose size is 
 * 'size'. Initialize the space to all bits zero.
 * 
 * @param nmemb The number of elements to allocate.
 * @param size  The size of each element, in bytes.
 * @return A pointer to the allocated space or NULL if not enough memory.
 */
void *
calloc(size_t nmemb, size_t size)
{
  size_t n = nmemb * size;
  void *ptr;

  if ((ptr = malloc(n)) == NULL)
    return NULL;
  
  memset(ptr, 0, n);

  return ptr;
}
