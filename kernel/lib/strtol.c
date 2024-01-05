#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static int tolower(int c)
{
  if (c >= 'A' && c <= 'Z') {
    return c - ('A' - 'a');
  }
  return c;
}

/**
 * Convert the initial portion of a string pointed to long int representation.
 * 
 * Discards any white-space characters (as specified by isspace) until the
 * first non-white-space character is found. Then takes as many characters as
 * possible to form a valid integer representation in the given radix and 
 * convert them to an integer.
 *
 * If the value of base is ​0​, the expected representation is that of an integer
 * constant, optionally preceded by a '+' or '-' sign, but not including an
 * integer suffix. If the base value is between 2 and 36, the expected format
 * is a sequence of letters and digits representing an integer with the
 * specified radix. The letters from 'a' (or 'A') through 'z' (or 'Z') are
 * ascribed the values 10 to 35. The sequence may optionally be preceded by a
 * '+' or '-' sign, and, if base is 16, the characters 0x or 0X.
 * 
 * If endptr is not NULL, it is set to point to the first unrecognized character
 * (or the terminating null character) in the input string.
 * 
 * If the string is empty or does not have the expected form, no conversion is
 * performed and endptr is set to point to nptr, provided that endptr is not
 * NULL.
 * 
 * @param nptr    Pointer to the string to be converted.
 * @param endptr  Pointer to store the final string position, or NULL.
 * @param base    Radix of the expected integer representation or 0 to
 *                auto-detect the numeric base.
 * 
 * @return  If successful, the converted value is returned. If no conversion
 *          could be performed, zero is returned. If the value is outside the
 *          range of representable values, LONG_MAX or LONG_MIN is returned
 *          (according to the sign of the value) and errno is set to ERANGE.
 */
long
strtol(const char *nptr, char **endptr, int base)
{
  static const char symbols[] = "0123456789abcdefghijklmnopqrstufvxyz";

  unsigned long value = 0;
  int negate = 0, overflow = 0;
  const char *s = nptr, *digit;

  if ((base < 0) || (base == 1) || (base > 32)) {
    // Unsupported base 
    if (endptr)
      *endptr = (char *) nptr;
    return 0;
  }

  // Skip whitespace
  while (strchr(" \t\r\n", *s))
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
    if (negate) {
      overflow = (value > -(unsigned long long) LONG_MIN);
    } else {
      overflow = (value > (unsigned long long) LONG_MAX);
    }
  }

  if (overflow) {
    // If the value is outside the valid range for the given type, return the
    // corresponding special value
    return negate ? LONG_MIN : LONG_MAX;
  }

  return negate ? -value : value;
}
