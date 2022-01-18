#include <stdlib.h>

/**
 * Compute a long integer absolute value.
 *
 * @param  i The long integer value.
 * @return The absolute value of i.
 */
long
labs(long i)
{
  return (i < 0) ? -i : i;
}
