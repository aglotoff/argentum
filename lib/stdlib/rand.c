#include <stdlib.h>

unsigned __stdlib_seed = 1;

/**
 * Compute a sequence of pseudo-random integers in the range 0 to RAND_MAX.
 * 
 * @return The next pseudo-random integer in the sequence.
 */
int
rand(void)
{
  return rand_r(&__stdlib_seed);
}
