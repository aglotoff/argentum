#include <stdlib.h>

/**
 * Initialize the pseudo-random number generator.
 * 
 * @param seed The seed for a new sequence of pseudo-random numbers.
 */
void
srand(unsigned seed)
{
  __randseed = seed;
}
