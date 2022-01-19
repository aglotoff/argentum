#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define BASE_MAX  36

unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
  static const char symbols[] = "0123456789abcdefghijklmnopqrstufvxyz";

  unsigned long n = 0;
  int neg = 0, overflow = 0;
  const char *s = nptr, *digit;

  if ((base < 0) || (base == 1) || (base > BASE_MAX)) {
    if (endptr)
      *endptr = (char *) nptr;
    return 0;
  }

  // Skip whitespace
  while (isspace(*s))
    s++;

  // Optional sign
  if ((*s == '-') || (*s == '+'))
    neg = *s++ == '-';
  
  if (!base) {
    // Try to determine base by the form of the integer constant.
    if (*s == '0') {
      if ((*++s == 'x') || (*s == 'X')) {
        base = 16;
        s++;
      } else {
        base = 8;
      }
    } else {
      base = 10;
    }
  } else if (base == 16) {
    // Skip an optional prefix
    if ((*s == '0') && ((*++s == 'x') || (*s == 'X')))
      s++;
  }

  for ( ; (digit = memchr(symbols, tolower(*s), base)) != NULL; s++) {
    unsigned long prev;

    if (overflow)
      continue;

    prev = n;
    n = n * base + (digit - symbols);

    if (prev > n) {
      overflow = 1;
      errno = ERANGE;
      n = ULONG_MAX;
    }
  }

  if (neg)
    n = -n;

  if (endptr)
    *endptr = (char *) s;
  
  return n;
}
