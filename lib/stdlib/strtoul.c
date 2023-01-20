#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Convert the initial portion of a string pointed to unsigned long int
 * representation.
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
 *          range of representable values, ULONG_MAX is returned and errno is
 *          set to ERANGE.
 */
unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
  return (unsigned long) __stdlib_parse_int(nptr, endptr, base,
                                            __STDLIB_PARSE_INT_SIGNED);
}
