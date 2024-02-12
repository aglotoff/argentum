#include <stdlib.h>

void *
_malloc_r (struct _reent *r, size_t n)
{
  return malloc (n);
}

void
_free_r (struct _reent *r, void *p)
{
  free (p);
}
