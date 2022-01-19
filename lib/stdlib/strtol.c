#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

long
strtol(const char *nptr, char **endptr, int base)
{
  const char *s = nptr;
  unsigned long un;
  int neg;

  // Skip whitespace
  while (isspace(*s))
    s++;

  neg = (*s == '-');

  un = strtoul(s, endptr, base);

  if (neg && (un <= LONG_MAX)) {
    // Negative integer overflow
    errno = ERANGE;
    return LONG_MIN;
  }

  if (!neg && (un > LONG_MAX)) {
    // Positive integer overflow
    errno = ERANGE;
    return LONG_MAX;
  }

  return (long) un;
}
