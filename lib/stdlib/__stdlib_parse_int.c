#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define BASE_MAX  36

// Common base for strtol, strtoll, strtoul, and strtoull functions
unsigned long long
__stdlib_parse_int(const char *nptr, char **endptr, int base, int flags)
{
  static const char symbols[] = "0123456789abcdefghijklmnopqrstufvxyz";

  unsigned long long value = 0;
  int negate = 0, overflow = 0;
  const char *s = nptr, *digit;

  if ((base < 0) || (base == 1) || (base > BASE_MAX)) {
    // Unsupported base 
    if (endptr)
      *endptr = (char *) nptr;
    return 0;
  }

  // Skip whitespace
  while (isspace(*s))
    s++;

  // Optional sign
  if ((*s == '-') || (*s == '+'))
    negate = (*s++ == '-');

  if (!base) {
    // Try to determine base by the form of the integer constant
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
    unsigned long old;

    if (overflow)
      continue;

    old = value;
    value = value * base + (digit - symbols);

    // If the value bedomes smaller, we definitely have overflow
    overflow = (value < old);
  }

  // Save pointer to the last unrecognized character
  if (endptr != NULL)
    *endptr = (char *) s;

  // Check for overflow depending on the integer type
  if (!overflow) {
    if (flags & __STDLIB_PARSE_INT_LONGLONG) {
      if (flags & __STDLIB_PARSE_INT_SIGNED) {
        if (negate) {
          overflow = (value > -(unsigned long long) LLONG_MIN);
        } else {
          overflow = (value > (unsigned long long) LLONG_MAX);
        }
      }
    } else {
      if (flags & __STDLIB_PARSE_INT_SIGNED) {
        if (negate) {
          overflow = (value > -(unsigned long long) LONG_MIN);
        } else {
          overflow = (value > (unsigned long long) LONG_MAX);
        }
      } else {
        overflow = (value > (unsigned long long) ULONG_MAX);
      }
    }
  }

  if (overflow) {
    errno = ERANGE;

    // If the value is outside the valid range for the given type, return the
    // corresponding special value
    if (flags & __STDLIB_PARSE_INT_LONGLONG) {
      if (flags & __STDLIB_PARSE_INT_SIGNED) {
        return negate ? LLONG_MIN : LLONG_MAX;
      }
      return ULLONG_MAX;
    } else {
      if (flags & __STDLIB_PARSE_INT_SIGNED) {
        return negate ? LONG_MIN : LONG_MAX;
      }
      return ULONG_MAX;
    }
  }

  return negate ? -value : value;
}
