#include <limits.h>
#include <stdlib.h>

int
atexit(void (*func)(void))
{
  if (__at_count >= ATEXIT_MAX)
    return -1;
  
  __at_funcs[__at_count++] = func;
  return 0;
}
