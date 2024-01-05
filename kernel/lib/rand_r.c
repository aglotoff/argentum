#include <stdlib.h>

int
rand_r(unsigned *seed)
{
  *seed = *seed * 1103515245 + 12345;
  return *seed & RAND_MAX;
}
