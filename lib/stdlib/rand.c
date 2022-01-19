#include <stdlib.h>

unsigned __randseed = 1;

/**
 * Compute a sequence of pseudo-random integers in the range 0 to RAND_MAX.
 * 
 * @return The next pseudo-random integer in the sequence.
 */
int
rand(void)
{
  __randseed = __randseed * 1103515245 + 12345;
  return (unsigned) (__randseed / 65536) & RAND_MAX;
}
