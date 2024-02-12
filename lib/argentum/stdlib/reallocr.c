#include <stdlib.h>

void *
_realloc_r (struct _reent *r, void *p, size_t n) _NOTHROW
{
  return realloc (p, n);
}
