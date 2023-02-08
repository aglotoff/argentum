#include <stdio.h>
#include <time.h>

#define BUF_SIZE  64 

/**
 * Convert date and time into a string.
 * 
 * Converts the specified broken-down time into a string in the form:
 * 'Sun Sep 16 01:03:52 1973\n\0'.
 * 
 * @param tm  Pointer to the broken-down time structure.
 * 
 * @return  A pointer to the string or NULL if the function is unsuccessful.
 */
char *
asctime(const struct tm *timeptr)
{
  static char buf[BUF_SIZE];

  if (strftime(buf, BUF_SIZE, "%a %b %d %H:%M:%S %Y\n", timeptr) == 0)
    return NULL;
  return buf;
}
